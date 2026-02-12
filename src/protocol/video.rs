use std::ffi::{CStr, CString};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use bluer::Address;
use libmpv2_sys::*;

unsafe extern "C" fn update_callback(cb_ctx: *mut std::os::raw::c_void) {
    let flag = &*(cb_ctx as *const AtomicBool);
    flag.store(true, Ordering::Release);
}

unsafe fn find_audio_device(ctx: *mut mpv_handle, mac_address: Address) -> Option<String> {
    // BlueZ/PipeWire sink names use underscores: B1_21_81_DD_B8_9B
    let mac_str = mac_address.to_string().replace(':', "_");

    let prop_name = CString::new("audio-device-list").ok()?;
    let mut node: mpv_node = std::mem::zeroed();
    let rc = mpv_get_property(
        ctx,
        prop_name.as_ptr(),
        mpv_format_MPV_FORMAT_NODE,
        &mut node as *mut mpv_node as *mut std::os::raw::c_void,
    );
    if rc < 0 {
        return None;
    }

    let result = (|| -> Option<String> {
        if node.format != mpv_format_MPV_FORMAT_NODE_ARRAY {
            return None;
        }
        let list = &*node.u.list;
        for i in 0..list.num {
            let entry = &*list.values.offset(i as isize);
            if entry.format != mpv_format_MPV_FORMAT_NODE_MAP {
                continue;
            }
            let map = &*entry.u.list;
            let mut name: Option<&str> = None;
            for j in 0..map.num {
                let key = CStr::from_ptr(*map.keys.offset(j as isize)).to_str().ok()?;
                if key == "name" {
                    let val = &*map.values.offset(j as isize);
                    if val.format == mpv_format_MPV_FORMAT_STRING {
                        name = Some(CStr::from_ptr(val.u.string).to_str().ok()?);
                    }
                }
            }
            if let Some(n) = name {
                if n.contains(&mac_str) {
                    return Some(n.to_string());
                }
            }
        }
        None
    })();

    mpv_free_node_contents(&mut node);
    result
}

pub struct VideoPlayer {
    ctx: *mut mpv_handle,
    render_ctx: *mut mpv_render_context,
    frame_ready: Arc<AtomicBool>,
    frame_ready_raw: *const AtomicBool,
}

unsafe impl Send for VideoPlayer {}

impl VideoPlayer {
    pub fn new(file_path: &str, mac_address: Address) -> Result<Self, String> {
        unsafe {
            let ctx = mpv_create();
            if ctx.is_null() {
                return Err("mpv_create() failed".into());
            }

            let set_opt = |name: &str, value: &str| -> Result<(), String> {
                let name_c = CString::new(name).map_err(|e| e.to_string())?;
                let value_c = CString::new(value).map_err(|e| e.to_string())?;
                let rc = mpv_set_option_string(ctx, name_c.as_ptr(), value_c.as_ptr());
                if rc < 0 {
                    return Err(format!("mpv_set_option_string({name}, {value}) failed: {rc}"));
                }
                Ok(())
            };

            set_opt("vo", "libmpv")?;
            set_opt("osd-level", "0")?;
            set_opt("sub", "no")?;
            set_opt("vf", "lavfi=[crop='min(iw,ih):min(iw,ih)']")?;

            let rc = mpv_initialize(ctx);
            if rc < 0 {
                mpv_terminate_destroy(ctx);
                return Err(format!("mpv_initialize() failed: {rc}"));
            }

            if let Some(device) = find_audio_device(ctx, mac_address) {
                log::info!("Using audio device: {}", device);
                let name_c = CString::new("audio-device").map_err(|e| e.to_string())?;
                let value_c = CString::new(device).map_err(|e| e.to_string())?;
                let rc = mpv_set_property_string(ctx, name_c.as_ptr(), value_c.as_ptr());
                if rc < 0 {
                    log::warn!("Failed to set audio device: {rc}");
                }
            } else {
                log::info!("Ditoo Pro audio sink not found, using default audio output");
            }

            let frame_ready = Arc::new(AtomicBool::new(false));
            let frame_ready_raw = Arc::into_raw(Arc::clone(&frame_ready));

            let api_type_str = MPV_RENDER_API_TYPE_SW.as_ptr() as *mut std::os::raw::c_void;
            let mut enable_advanced: i32 = 1;
            let mut create_params = [
                mpv_render_param {
                    type_: mpv_render_param_type_MPV_RENDER_PARAM_API_TYPE,
                    data: api_type_str,
                },
                mpv_render_param {
                    type_: mpv_render_param_type_MPV_RENDER_PARAM_ADVANCED_CONTROL,
                    data: &mut enable_advanced as *mut i32 as *mut std::os::raw::c_void,
                },
                mpv_render_param {
                    type_: mpv_render_param_type_MPV_RENDER_PARAM_INVALID,
                    data: std::ptr::null_mut(),
                },
            ];

            let mut render_ctx: *mut mpv_render_context = std::ptr::null_mut();
            let rc = mpv_render_context_create(
                &mut render_ctx,
                ctx,
                create_params.as_mut_ptr(),
            );
            if rc < 0 {
                // Reconstruct the leaked Arc before returning
                let _ = Arc::from_raw(frame_ready_raw);
                mpv_terminate_destroy(ctx);
                return Err(format!("mpv_render_context_create() failed: {rc}"));
            }

            mpv_render_context_set_update_callback(
                render_ctx,
                Some(update_callback),
                frame_ready_raw as *mut std::os::raw::c_void,
            );

            let loadfile = CString::new("loadfile").map_err(|e| e.to_string())?;
            let file_path_c = CString::new(file_path).map_err(|e| e.to_string())?;
            let mut cmd_args: [*const std::os::raw::c_char; 3] = [
                loadfile.as_ptr(),
                file_path_c.as_ptr(),
                std::ptr::null(),
            ];
            let rc = mpv_command(ctx, cmd_args.as_mut_ptr());
            if rc < 0 {
                mpv_render_context_set_update_callback(render_ctx, None, std::ptr::null_mut());
                mpv_render_context_free(render_ctx);
                let _ = Arc::from_raw(frame_ready_raw);
                mpv_terminate_destroy(ctx);
                return Err(format!("mpv_command(loadfile) failed: {rc}"));
            }

            Ok(VideoPlayer {
                ctx,
                render_ctx,
                frame_ready,
                frame_ready_raw,
            })
        }
    }

    pub fn render_frame(&self) -> Option<[u8; 768]> {
        if !self.frame_ready.swap(false, Ordering::AcqRel) {
            return None;
        }

        let flags = unsafe { mpv_render_context_update(self.render_ctx) };
        if flags & (mpv_render_update_flag_MPV_RENDER_UPDATE_FRAME as u64) == 0 {
            return None;
        }

        // rgb0 = RGBX, 4 bytes per pixel
        let mut buffer = [0u8; 16 * 16 * 4];
        let mut size: [i32; 2] = [16, 16];
        let format = CString::new("rgb0").ok()?;
        let mut stride: usize = 16 * 4;

        let mut render_params = [
            mpv_render_param {
                type_: mpv_render_param_type_MPV_RENDER_PARAM_SW_SIZE,
                data: size.as_mut_ptr() as *mut std::os::raw::c_void,
            },
            mpv_render_param {
                type_: mpv_render_param_type_MPV_RENDER_PARAM_SW_FORMAT,
                data: format.as_ptr() as *mut std::os::raw::c_void,
            },
            mpv_render_param {
                type_: mpv_render_param_type_MPV_RENDER_PARAM_SW_STRIDE,
                data: &mut stride as *mut usize as *mut std::os::raw::c_void,
            },
            mpv_render_param {
                type_: mpv_render_param_type_MPV_RENDER_PARAM_SW_POINTER,
                data: buffer.as_mut_ptr() as *mut std::os::raw::c_void,
            },
            mpv_render_param {
                type_: mpv_render_param_type_MPV_RENDER_PARAM_INVALID,
                data: std::ptr::null_mut(),
            },
        ];

        let rc = unsafe {
            mpv_render_context_render(self.render_ctx, render_params.as_mut_ptr())
        };
        if rc < 0 {
            return None;
        }

        // Convert RGBX (4 bpp) to RGB (3 bpp)
        let mut rgb = [0u8; 768];
        for i in 0..256 {
            rgb[i * 3] = buffer[i * 4];
            rgb[i * 3 + 1] = buffer[i * 4 + 1];
            rgb[i * 3 + 2] = buffer[i * 4 + 2];
        }
        Some(rgb)
    }

    /// Poll mpv events. Returns `true` if playback has ended.
    pub fn poll_events(&self) -> bool {
        loop {
            let event = unsafe { &*mpv_wait_event(self.ctx, 0.0) };
            if event.event_id == mpv_event_id_MPV_EVENT_NONE {
                return false;
            }
            if event.event_id == mpv_event_id_MPV_EVENT_END_FILE
                || event.event_id == mpv_event_id_MPV_EVENT_SHUTDOWN
            {
                return true;
            }
        }
    }
}

impl Drop for VideoPlayer {
    fn drop(&mut self) {
        unsafe {
            mpv_render_context_set_update_callback(
                self.render_ctx,
                None,
                std::ptr::null_mut(),
            );
            mpv_render_context_free(self.render_ctx);
            mpv_terminate_destroy(self.ctx);
            let _ = Arc::from_raw(self.frame_ready_raw);
        }
    }
}
