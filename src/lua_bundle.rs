//! Resolve local, literal Lua imports on the host. The device receives text only.
use std::{
  collections::{BTreeSet, HashMap},
  error::Error,
  fs,
  path::{Path, PathBuf},
};

type Result<T> = std::result::Result<T, Box<dyn Error>>;

#[derive(Clone, Copy)]
struct Token<'a> {
  text: &'a str,
  start: usize,
  end: usize,
}

fn is_field(t: &[Token<'_>], i: usize) -> bool {
  i > 0 && (t[i - 1].text == ":" || (t[i - 1].text == "." && (i < 2 || t[i - 2].text != ".")))
}

fn field_after<'a>(t: &[Token<'a>], i: usize) -> Option<&'a str> {
  let field = t.get(i + 2)?.text;
  (t.get(i + 1)?.text == "."
    && field
      .bytes()
      .next()
      .is_some_and(|b| b.is_ascii_alphabetic() || b == b'_'))
  .then_some(field)
}

fn bracket(bytes: &[u8], at: usize) -> Option<usize> {
  if bytes.get(at) != Some(&b'[') {
    return None;
  }
  let mut end = at + 1;
  while bytes.get(end) == Some(&b'=') {
    end += 1;
  }
  (bytes.get(end) == Some(&b'[')).then_some(end - at - 1)
}

fn long_end(source: &str, at: usize, equals: usize) -> Result<usize> {
  let end = format!("]{}]", "=".repeat(equals));
  let begin = at + equals + 2;
  source[begin..]
    .find(&end)
    .map(|i| begin + i + end.len())
    .ok_or_else(|| "Unterminated Lua long string/comment".into())
}

// Keep strings opaque, including escaped quotes, escaped newlines and long
// brackets. Imports inside comments or strings must never read host files.
fn tokens(source: &str) -> Result<Vec<Token<'_>>> {
  let b = source.as_bytes();
  let mut out = Vec::new();
  let mut i = 0;
  while i < b.len() {
    if b[i].is_ascii_whitespace() {
      i += 1;
      continue;
    }
    if b[i..].starts_with(b"--") {
      i += 2;
      if let Some(n) = bracket(b, i) {
        i = long_end(source, i, n)?;
      } else {
        while i < b.len() && !matches!(b[i], b'\n' | b'\r') {
          i += 1;
        }
      }
      continue;
    }
    let start = i;
    if matches!(b[i], b'\'' | b'"') {
      let quote = b[i];
      i += 1;
      loop {
        if i == b.len() {
          return Err("Unterminated Lua string".into());
        }
        let ch = b[i];
        i += 1;
        if ch == quote {
          break;
        }
        if ch == b'\\' && i < b.len() {
          i += 1;
        }
      }
    } else if let Some(n) = bracket(b, i) {
      i = long_end(source, i, n)?;
    } else if b[i].is_ascii_alphanumeric() || b[i] == b'_' {
      i += 1;
      while i < b.len() && (b[i].is_ascii_alphanumeric() || b[i] == b'_') {
        i += 1;
      }
    } else {
      i += source[i..]
        .chars()
        .next()
        .ok_or("Invalid Lua token")?
        .len_utf8();
    }
    out.push(Token {
      text: &source[start..i],
      start,
      end: i,
    });
  }
  Ok(out)
}

fn gap(out: &mut String, text: &str, next: &str) {
  if text.is_empty() {
    return;
  }
  let Some(left) = out.as_bytes().last().copied() else {
    return;
  };
  let right = next.as_bytes()[0];
  let word = |b: u8| b.is_ascii_alphanumeric() || b == b'_' || b >= 128;
  // Preserve token boundaries, including number exponents, compound operators,
  // comments and long brackets. Literal contents are copied without changes.
  if (word(left) && word(right))
    || (left == b'.' && (right == b'.' || right.is_ascii_digit()))
    || (left.is_ascii_digit() && right == b'.')
    || (matches!(left, b'e' | b'E' | b'p' | b'P') && matches!(right, b'+' | b'-'))
    || matches!(
      (left, right),
      (b'-', b'-')
        | (b'/', b'/')
        | (b'<', b'<')
        | (b'>', b'>')
        | (b'=', b'=')
        | (b'~', b'=')
        | (b'<', b'=')
        | (b'>', b'=')
        | (b':', b':')
        | (b'[', b'[')
        | (b'[', b'=')
        | (b'=', b'[')
        | (b']', b'=')
        | (b'=', b']')
    )
  {
    out.push(' ');
  }
}

fn module_path(parent: &Path, quoted: &str) -> Result<PathBuf> {
  let name = quoted
    .get(1..quoted.len().saturating_sub(1))
    .unwrap_or_default();
  if !matches!(quoted.as_bytes().first(), Some(b'\'' | b'"'))
    || name.is_empty()
    || !name
      .bytes()
      .all(|b| b.is_ascii_alphanumeric() || b"_./-".contains(&b))
    || Path::new(name).is_absolute()
  {
    return Err(
      "require expects a literal local module name/path (no escapes or absolute paths)".into(),
    );
  }
  let relative = if name.contains('/') || name.ends_with(".lua") {
    name.to_owned()
  } else {
    if name.split('.').any(str::is_empty) {
      return Err("Dotted Lua module names cannot contain empty components".into());
    }
    name.replace('.', "/")
  };
  let relative = if relative.ends_with(".lua") {
    relative
  } else {
    relative + ".lua"
  };
  Ok(parent.join(relative))
}

#[derive(Default)]
struct Builder {
  ids: HashMap<PathBuf, usize>,
  stack: Vec<PathBuf>,
  modules: Vec<String>,
  original: Vec<u8>,
  bytes: usize,
  reserved: bool,
}
impl Builder {
  fn load(&mut self, file: &Path) -> Result<usize> {
    let path = fs::canonicalize(file).map_err(|e| format!("{}: {e}", file.display()))?;
    if self.stack.contains(&path) {
      return Err(
        format!(
          "Cyclic Lua import: {} -> {}",
          self
            .stack
            .iter()
            .map(|p| p.display().to_string())
            .collect::<Vec<_>>()
            .join(" -> "),
          path.display()
        )
        .into(),
      );
    }
    if let Some(id) = self.ids.get(&path) {
      return Ok(*id);
    }
    if self.modules.len() >= 32 {
      return Err("Lua bundle exceeds 32 source files".into());
    }
    if fs::metadata(&path)?.len() > 262144 {
      return Err("Lua source file exceeds 256 KiB".into());
    }
    let source = fs::read_to_string(&path)?;
    self.bytes += source.len();
    if self.bytes > 262144 {
      return Err("Lua module sources exceed 256 KiB".into());
    }
    let id = self.modules.len() + 1;
    if id == 1 {
      self.original = source.as_bytes().to_vec();
    }
    self.ids.insert(path.clone(), id);
    self.modules.push(String::new());
    self.stack.push(path.clone());
    let t = tokens(&source).map_err(|e| format!("{}: {e}", path.display()))?;
    let mut out = String::new();
    let mut previous = 0;
    let mut i = 0;
    while i < t.len() {
      gap(&mut out, &source[previous..t[i].start], t[i].text);
      self.reserved |= matches!(t[i].text, "__dm" | "__dr");
      if t[i].text == "require" && !is_field(&t, i) {
        if t.get(i + 1).map(|t| t.text) != Some("(") || t.get(i + 3).map(|t| t.text) != Some(")") {
          return Err(format!("{}: use require(\"local.module\") with one literal path; dynamic/aliased require is unsupported", path.display()).into());
        }
        let dependency = module_path(path.parent().ok_or("Module has no parent")?, t[i + 2].text)?;
        let imported = self.load(&dependency)?;
        out.push_str(&format!("__dr({imported})"));
        previous = t[i + 3].end;
        i += 4;
      } else {
        out.push_str(t[i].text);
        previous = t[i].end;
        i += 1;
      }
    }
    out.push('\n');
    self.modules[id - 1] = out;
    self.stack.pop();
    Ok(id)
  }
}

#[derive(Clone, PartialEq, Default)]
enum Need {
  #[default]
  Unused,
  Fields(BTreeSet<String>),
  All,
}
impl Need {
  fn merge(&mut self, incoming: Self) -> bool {
    let before = self.clone();
    match (&mut *self, incoming) {
      (_, Need::Unused) | (Need::All, _) => {}
      (slot, Need::All) => *slot = Need::All,
      (Need::Fields(a), Need::Fields(b)) => a.extend(b),
      (slot, fields) => *slot = fields,
    }
    *self != before
  }
}

struct Pure {
  name: String,
  functions: Vec<(String, String)>,
}
impl Pure {
  // Only an empty local table, named function declarations and its return are
  // accepted. Everything else retains ordinary module initialization semantics.
  fn parse(source: &str) -> Option<Self> {
    let t = tokens(source).ok()?;
    if t.len() < 7
      || t[0].text != "local"
      || t[2].text != "="
      || t[3].text != "{"
      || t[4].text != "}"
    {
      return None;
    }
    let name = t[1].text;
    let mut functions = Vec::new();
    let mut names = BTreeSet::new();
    let mut i = 5;
    loop {
      while t.get(i)?.text == ";" {
        i += 1;
      }
      if t.get(i)?.text != "function" {
        break;
      }
      if t.get(i + 1)?.text != name || t.get(i + 2)?.text != "." || t.get(i + 4)?.text != "(" {
        return None;
      }
      let field = t[i + 3].text.to_owned();
      if !names.insert(field.clone()) {
        return None;
      }
      let start = t[i].start;
      let mut depth = 1;
      i += 5;
      while depth > 0 {
        match t.get(i)?.text {
          "function" | "if" | "do" | "repeat" => depth += 1,
          "end" | "until" => depth -= 1,
          _ => {}
        }
        i += 1;
      }
      functions.push((field, source[start..t[i - 1].end].to_owned()));
    }
    if t.get(i)?.text != "return"
      || t.get(i + 1)?.text != name
      || t[i + 2..].iter().any(|t| t.text != ";")
    {
      return None;
    }
    Some(Self {
      name: name.to_owned(),
      functions,
    })
  }
  fn render(&self, need: &Need) -> Result<String> {
    let mut fields = match need {
      Need::Fields(fields) => fields.clone(),
      _ => self
        .functions
        .iter()
        .map(|(name, _)| name.clone())
        .collect(),
    };
    loop {
      let before = fields.len();
      for (name, body) in &self.functions {
        if !fields.contains(name) {
          continue;
        }
        let t = tokens(body)?;
        // Skip the function's own receiver in its declaration.
        for i in 4..t.len() {
          if t[i].text != self.name || is_field(&t, i) {
            continue;
          }
          if let Some(field) = field_after(&t, i) {
            fields.insert(field.to_owned());
          } else {
            fields.extend(self.functions.iter().map(|(name, _)| name.clone()));
          }
        }
      }
      if fields.len() == before {
        break;
      }
    }
    let mut out = format!("local {}={{}};", self.name);
    for (name, body) in &self.functions {
      if fields.contains(name) {
        out.push_str(body);
        out.push(' ');
      }
    }
    out.push_str(&format!("return {}\n", self.name));
    Ok(out)
  }
}

fn imports(source: &str, pure: &[Option<Pure>]) -> Result<(String, Vec<(usize, Need)>)> {
  let t = tokens(source)?;
  let mut edges = Vec::new();
  let mut removed = Vec::new();
  for i in 0..t.len() {
    if t[i].text != "__dr" {
      continue;
    }
    let id: usize = t
      .get(i + 2)
      .ok_or("Incomplete bundled import")?
      .text
      .parse()?;
    let end = i + 3;
    let mut need = Need::All;
    let next = t.get(end + 1).map(|t| t.text).unwrap_or("");
    // Only analyze a single local binding whose RHS ends at this call. Chained
    // calls, operators and multiple assignments retain the complete namespace.
    let boundary = next.is_empty()
      || next == ";"
      || (next
        .bytes()
        .next()
        .is_some_and(|b| b.is_ascii_alphabetic() || b == b'_')
        && !matches!(next, "and" | "or"));
    if pure[id - 1].is_some()
      && i >= 3
      && t[i - 3].text == "local"
      && t[i - 1].text == "="
      && boundary
    {
      let alias = t[i - 2].text;
      let mut fields = BTreeSet::new();
      let mut all = false;
      for j in 0..t.len() {
        if j == i - 2 || t[j].text != alias || is_field(&t, j) {
          continue;
        }
        if let Some(field) = field_after(&t, j) {
          fields.insert(field.to_owned());
        } else {
          all = true;
        }
      }
      need = if all {
        Need::All
      } else if fields.is_empty() {
        Need::Unused
      } else {
        Need::Fields(fields)
      };
      if need == Need::Unused {
        removed.push((t[i - 3].start, t[end].end));
      }
    } else if pure[id - 1].is_some() {
      if let Some(field) = field_after(&t, end) {
        need = Need::Fields([field.to_owned()].into());
      }
    }
    edges.push((id, need));
  }
  let mut out = source.to_owned();
  for (start, end) in removed.into_iter().rev() {
    out.replace_range(start..end, " ");
  }
  Ok((out, edges))
}

fn link(modules: &[String]) -> Result<String> {
  let pure: Vec<_> = modules.iter().map(|s| Pure::parse(s)).collect();
  let mut needs = vec![Need::Unused; modules.len()];
  needs[0] = Need::All;
  let mut output = vec![String::new(); modules.len()];
  loop {
    let mut changed = false;
    for i in 0..modules.len() {
      if needs[i] == Need::Unused {
        continue;
      }
      let source = match &pure[i] {
        Some(p) => p.render(&needs[i])?,
        None => modules[i].clone(),
      };
      let (body, edges) = imports(&source, &pure)?;
      output[i] = body;
      for (id, need) in edges {
        changed |= needs[id - 1].merge(need);
      }
    }
    if !changed {
      break;
    }
  }
  let static_only = (1..modules.len()).all(|i| needs[i] == Need::Unused || pure[i].is_some());
  if needs[1..].iter().all(|n| *n == Need::Unused) {
    return Ok(output[0].clone());
  }
  let mut out = if static_only {
    String::from("local __dm={}\n")
  } else {
    String::from("local __dm={}\nlocal function __dr(i)\nlocal m=__dm[i]\nif not m[2] then local v=m[1]();if v==nil then v=true end;m[1],m[2]=v,true end\nreturn m[1]\nend\n")
  };
  for i in 1..modules.len() {
    if needs[i] == Need::Unused {
      continue;
    }
    let body = if static_only {
      static_imports(&output[i])?
    } else {
      output[i].clone()
    };
    if static_only {
      out.push_str(&format!("__dm[{}]=(function()\n{}end)()\n", i + 1, body));
    } else {
      out.push_str(&format!("__dm[{}]={{function()\n{}end}}\n", i + 1, body));
    }
  }
  out.push_str(&if static_only {
    static_imports(&output[0])?
  } else {
    output[0].clone()
  });
  Ok(out)
}

fn static_imports(source: &str) -> Result<String> {
  let t = tokens(source)?;
  let mut out = source.to_owned();
  for i in (0..t.len()).rev() {
    if t[i].text == "__dr" {
      out.replace_range(
        t[i].start..t[i + 3].end,
        &format!("__dm[{}]", t[i + 2].text),
      );
    }
  }
  Ok(out)
}

/// Bundle literal imports, caching each module's return value once per app VM.
/// Files without imports stay unchanged if they fit; oversized files are also
/// compacted. Errors precede device access.
pub fn bundle(file: &Path) -> Result<Vec<u8>> {
  let mut builder = Builder::default();
  builder.load(file)?;
  let source = if builder.modules.len() == 1 {
    if builder.original.len() <= crate::lua::APP_SOURCE_LIMIT {
      builder.original
    } else {
      builder.modules.remove(0).into_bytes()
    }
  } else {
    if builder.reserved {
      return Err("Bundled Lua reserves identifiers __dm and __dr".into());
    }
    link(&builder.modules)?.into_bytes()
  };
  if source.is_empty() || source.len() > crate::lua::APP_SOURCE_LIMIT {
    return Err(
      format!(
        "Lua bundle is {} bytes; device limit is 1..={} bytes",
        source.len(),
        crate::lua::APP_SOURCE_LIMIT
      )
      .into(),
    );
  }
  Ok(source)
}

#[cfg(test)]
mod tests {
  use super::*;
  use std::sync::atomic::{AtomicUsize, Ordering};
  struct Files(PathBuf);
  impl Files {
    fn new() -> Self {
      static ID: AtomicUsize = AtomicUsize::new(0);
      let p = std::env::temp_dir().join(format!(
        "ditoo-bundle-{}-{}",
        std::process::id(),
        ID.fetch_add(1, Ordering::Relaxed)
      ));
      fs::create_dir(&p).unwrap();
      Self(p)
    }
    fn write(&self, name: &str, source: &str) -> PathBuf {
      let p = self.0.join(name);
      fs::create_dir_all(p.parent().unwrap()).unwrap();
      fs::write(&p, source).unwrap();
      p
    }
  }
  impl Drop for Files {
    fn drop(&mut self) {
      let _ = fs::remove_dir_all(&self.0);
    }
  }
  #[test]
  fn plain_source_and_quoted_imports_stay_opaque() {
    let f = Files::new();
    let source = "-- require('missing')\nreturn [==[require('also-missing')]==]";
    assert_eq!(
      bundle(&f.write("main.lua", source)).unwrap(),
      source.as_bytes()
    );
    f.write("lib.lua", "return 'ok'");
    let p=f.write("main.lua","local m=require('lib');local s=\"escaped \\\" require('missing')\";return [=[a\n  b]=]..s..m");
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert!(b.contains("[=[a\n  b]=]"));
    assert!(b.contains("require('missing')"));
  }
  #[test]
  fn shared_dependencies_are_deduplicated_relative_to_each_file() {
    let f = Files::new();
    f.write("lib/shared.lua", "return {marker=987654}");
    f.write("lib/first.lua", "return require('shared')");
    let p = f.write(
      "main.lua",
      "local a=require('lib.first');local b=require('./lib/shared.lua');return a==b",
    );
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert_eq!(b.matches("marker=987654").count(), 1);
    assert!(b.contains("return a==b"));
  }
  #[test]
  fn cycles_missing_dynamic_and_shadowed_imports_fail_before_upload() {
    let f = Files::new();
    f.write("a.lua", "return require('b')");
    let p = f.write("b.lua", "return require('a')");
    assert!(bundle(&p).unwrap_err().to_string().contains("Cyclic"));
    for source in [
      "return require('missing')",
      "return require(name)",
      "local r=require",
      "local require=function() end",
      "return require('../missing')",
    ] {
      assert!(bundle(&f.write("main.lua", source)).is_err(), "{source}");
    }
    f.write("valid.lua", "return 1");
    assert!(bundle(&f.write("main.lua", "local __dm={};return require('valid')")).is_err());
  }
  #[test]
  fn whitespace_cannot_create_operators_comments_or_literals() {
    let f = Files::new();
    f.write("lib.lua", "return 1");
    let p=f.write("main.lua","local m=require('lib');local n=0x1e - 2;return 1 .. .5, n - --comment\n - m, t[ [=[x]=] ], 'a  b'");
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert!(b.contains("0x1e -2"));
    assert!(b.contains("1 .. .5"));
    assert!(b.contains("n- -m"));
    assert!(b.contains("t[ [=[x]=]"));
    assert!(b.contains("'a  b'"));
  }
  #[test]
  fn output_limit_catches_large_dependencies() {
    let f = Files::new();
    f.write("big.lua", &format!("return '{}'", "x".repeat(8192)));
    let p = f.write("main.lua", "return require('big')");
    assert!(bundle(&p).unwrap_err().to_string().contains("device limit"));
  }
  #[test]
  fn oversized_standalone_source_compacts_without_changing_literals() {
    let f = Files::new();
    let p = f.write(
      "main.lua",
      &format!("--{}\nreturn 'a  b'", "comment".repeat(1200)),
    );
    assert_eq!(bundle(&p).unwrap(), b"return'a  b'\n");
    let p = f.write("main.lua", &format!("return '{}'", "x".repeat(8192)));
    assert!(bundle(&p).unwrap_err().to_string().contains("device limit"));
  }
  #[test]
  fn unused_pure_imports_exports_and_their_dependencies_are_removed() {
    let f = Files::new();
    f.write("dead.lua", "error('dead initializer')");
    f.write("pure.lua","local m={} function m.used() return m.helper() end function m.helper() return 7 end function m.unused() return require('dead') end return m");
    let p = f.write("main.lua", "local m=require('pure');return m.used()");
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert!(b.contains("function m.used()") && b.contains("function m.helper()"));
    assert!(!b.contains("unused") && !b.contains("dead initializer") && !b.contains("__dr"));
    let p = f.write("main.lua", "do end local m=require('pure') return 1");
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert!(!b.contains("__dm") && !b.contains("function") && !b.contains("endreturn"));
  }
  #[test]
  fn empty_tables_and_empty_statements_do_not_disable_tree_shaking() {
    let f = Files::new();
    f.write("empty.lua", "local m={};return m;");
    f.write(
      "pure.lua",
      "local m={};function m.a() return 1 end;function m.b() return 2 end;return m;",
    );
    let p = f.write(
      "main.lua",
      "local unused=require('empty');return require('pure').a()",
    );
    let b = String::from_utf8(bundle(&p).unwrap()).unwrap();
    assert!(
      b.contains("function m.a()")
        && !b.contains("function m.b()")
        && !b.contains("unused")
        && !b.contains("__dr")
    );
  }
  #[test]
  fn escaping_namespaces_and_initializers_keep_their_semantics() {
    let f = Files::new();
    f.write(
      "pure.lua",
      "local m={} function m.a() return 1 end function m.b() return 2 end return m",
    );
    for source in [
      "local m=require('pure');return m",
      "local m=require('pure');return m[key]",
      "return require('pure'):a()",
      "local m=require('pure');m=other;return m.a()",
      "local m=require('pure');return m .. 'text'",
      "return require('pure') .. 'text'",
    ] {
      let b = String::from_utf8(bundle(&f.write("main.lua", source)).unwrap()).unwrap();
      assert!(
        b.contains("function m.a()") && b.contains("function m.b()"),
        "{b}"
      );
    }
    f.write("effect.lua", "count=(count or 0)+1;return {}");
    let b = String::from_utf8(
      bundle(&f.write("main.lua", "local unused=require('effect');return count")).unwrap(),
    )
    .unwrap();
    assert!(b.contains("count=(count or 0)+1") && b.contains("local unused=__dr("));
  }
  #[test]
  fn imports_union_exports_and_keep_transitive_function_bodies() {
    let f = Files::new();
    f.write("pure.lua","local m={} function m.a() for i=1,1 do while false do end end repeat if true then do end end until true;return function() return m.b() end end function m.b() return 2 end function m.c() return 3 end function m.dead() return 'DROP' end return m");
    let b = String::from_utf8(
      bundle(&f.write(
        "main.lua",
        "local a=require('pure');local b=require('pure');return a.a()(),b.c()",
      ))
      .unwrap(),
    )
    .unwrap();
    for field in ["a", "b", "c"] {
      assert!(b.contains(&format!("function m.{field}()")));
    }
    assert!(!b.contains("DROP"));
    assert_eq!(b.matches("local m={}").count(), 1);
  }
  #[test]
  fn narrow_ui_import_does_not_include_screen_or_runtime_loader() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let f = Files::new();
    f.write(
      "ui.lua",
      &fs::read_to_string(root.join("lua/ui.lua")).unwrap(),
    );
    let b = String::from_utf8(
      bundle(&f.write(
        "main.lua",
        "local ui=require('ui');ui.center(0,'HI',0xffffff)",
      ))
      .unwrap(),
    )
    .unwrap();
    assert!(b.len() < 300, "{} bytes: {b}", b.len());
    for unused in ["scroll", "screen", "indicator", "elapsed", "__dr"] {
      assert!(!b.contains(unused));
    }
  }
  #[test]
  fn tv_app_and_another_ui_client_fit_without_firmware_changes() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    for name in ["tv-keyboard", "ui-demo"] {
      let source = bundle(&root.join(format!("examples/lua/{name}.lua"))).unwrap();
      assert!(source.len() <= crate::lua::APP_SOURCE_LIMIT - 288);
    }
  }
}
