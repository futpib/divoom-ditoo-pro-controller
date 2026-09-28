//! Offline selector reference extracted from the Android app and captured replies.
use serde::{Deserialize, Serialize};
use std::error::Error;

#[derive(Debug, Deserialize, Serialize)]
struct Entry {
  id: u16,
  name: String,
}

#[derive(Debug, Deserialize, Serialize)]
struct Category {
  category: String,
  command: String,
  note: String,
  source: String,
  status: String,
  entries: Vec<Entry>,
}

fn catalogue() -> Result<Vec<Category>, Box<dyn Error>> {
  Ok(serde_json::from_str(include_str!(
    "../data/device-ids.json"
  ))?)
}

pub fn show(category: Option<&str>, json: bool) -> Result<(), Box<dyn Error>> {
  let items = catalogue()?;
  let selected = if let Some(name) = category {
    Some(
      items
        .iter()
        .find(|item| item.category == name)
        .ok_or_else(|| format!("Unknown ID category {name}; run ids to list categories"))?,
    )
  } else {
    None
  };
  if json {
    println!(
      "{}",
      if let Some(item) = selected {
        serde_json::to_string_pretty(item)?
      } else {
        serde_json::to_string_pretty(&items)?
      }
    );
  } else if let Some(item) = selected {
    println!(
      "{} — {}\n{}\n{}\n",
      item.category, item.command, item.status, item.note
    );
    if item.entries.is_empty() {
      println!("No established ID/name mapping is available.");
    } else {
      println!("#\tID\tName");
      for (index, entry) in item.entries.iter().enumerate() {
        println!("{}\t{}\t{}", index + 1, entry.id, entry.name);
      }
    }
    println!("\nSource: {}", item.source);
  } else {
    println!("Offline ID reference; app-defined IDs are not a device capability query.\nUse ids CATEGORY to list values, or --json for structured output.\n");
    println!("#\tCategory\tKnown IDs\tCommand");
    for (index, item) in items.iter().enumerate() {
      println!(
        "{}\t{}\t{}\t{}",
        index + 1,
        item.category,
        item.entries.len(),
        item.command
      );
    }
  }
  Ok(())
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn source_mappings_and_dynamic_ids_remain_distinct() -> Result<(), Box<dyn Error>> {
    let items = catalogue()?;
    let find = |name| {
      items
        .iter()
        .find(|c| c.category == name)
        .ok_or("Missing ID category")
    };
    let games = find("games")?;
    assert_eq!(games.entries.len(), 15);
    assert_eq!(
      (games.entries[0].id, games.entries[0].name.as_str()),
      (1, "Pixel Slot")
    );
    assert_eq!(
      (games.entries[14].id, games.entries[14].name.as_str()),
      (15, "Pixel Battle")
    );
    let noise = find("noise-meter")?;
    assert_eq!(
      (noise.entries[1].id, noise.entries[1].name.as_str()),
      (2, "stop")
    );
    assert!(find("sd-tracks")?.entries.is_empty());
    let mut names = std::collections::HashSet::new();
    for item in items {
      assert!(names.insert(item.category));
      let mut ids = std::collections::HashSet::new();
      for entry in item.entries {
        assert!(ids.insert(entry.id));
      }
    }
    Ok(())
  }
}
