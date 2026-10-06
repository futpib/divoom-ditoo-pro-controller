package dev.ditoo.tvprobe;
import android.bluetooth.*;
import android.content.*;
import android.os.*;
import android.util.Log;
import java.io.*;
import java.util.*;
public class Control extends BroadcastReceiver {
  public void onReceive(Context c, Intent i) {
    new Session(c, goAsync(), i.getStringExtra("message")).start();
  }
  static class Session extends BluetoothGattCallback {
    final Context context;
    final PendingResult pending;
    final String message;
    final Handler h = new Handler(Looper.getMainLooper());
    BluetoothGatt g;
    BluetoothGattCharacteristic write;
    boolean done, discovering, subscribing;
    int stage, sequence, offset, maintenanceStep;
    byte[] wire, buffer = new byte[0];
    Session(Context c, PendingResult p, String m) {
      context = c;
      pending = p;
      message = m;
    }
    void start() {
      try {
        h.postDelayed(() -> finish("timeout"), 9000);
        g = context.getSystemService(BluetoothManager.class)
                .getAdapter()
                .getRemoteDevice("D1:21:81:DD:B8:9B")
                .connectGatt(context, false, this,
                             BluetoothDevice.TRANSPORT_LE);
      } catch (Throwable t) {
        finish(t.toString());
      }
    }
    void finish(String s) {
      if (done)
        return;
      done = true;
      Log.i("DitooTVControl", s);
      if (g != null)
        g.close();
      pending.finish();
    }
    public void onConnectionStateChange(BluetoothGatt g, int status,
                                        int state) {
      if (status != 0) {
        finish("connect-status=" + status);
        return;
      }
      if (state == 2)
        g.requestMtu(247);
    }
    public void onMtuChanged(BluetoothGatt g, int mtu, int status) {
      if (done || discovering) return;
      discovering = true;
      if (status == 0 && mtu >= 247) g.discoverServices();
      else finish("mtu=" + mtu + " status=" + status);
    }
    public void onServicesDiscovered(BluetoothGatt g, int status) {
      if (done || subscribing) return;
      subscribing = true;
      try {
        if (status != 0)
          throw new Exception("services=" + status);
        BluetoothGattService svc = g.getService(
            UUID.fromString("49535343-fe7d-4ae5-8fa9-9fafd205e455"));
        write = svc.getCharacteristic(
            UUID.fromString("49535343-8841-43f4-a8d4-ecbe34729bb3"));
        BluetoothGattCharacteristic notify = svc.getCharacteristic(
            UUID.fromString("49535343-1e4d-4bd9-ba61-23c647249616"));
        g.setCharacteristicNotification(notify, true);
        BluetoothGattDescriptor d = notify.getDescriptor(
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"));
        d.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
        if (!g.writeDescriptor(d))
          throw new Exception("subscribe refused");
      } catch (Throwable t) {
        finish(t.toString());
      }
    }
    public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor d,
                                  int status) {
      if (status != 0) {
        finish("subscribe=" + status);
        return;
      }
      stage = 1;
      send(new byte[] {9}, 0x0101);
    }
    void send(byte[] payload, int seq) {
      if (done)
        return;
      sequence = seq;
      int n = payload.length + 7;
      wire = new byte[payload.length + 13];
      byte[] head = {(byte)0xfe,
                     (byte)0xef,
                     (byte)0xaa,
                     0x55,
                     (byte)n,
                     (byte)(n >> 8),
                     (byte)(seq >> 8),
                     (byte)seq,
                     0,
                     0,
                     0};
      System.arraycopy(head, 0, wire, 0, 11);
      System.arraycopy(payload, 0, wire, 11, payload.length);
      int sum = 0;
      for (int k = 4; k < wire.length - 2; k++)
        sum += wire[k] & 255;
      wire[wire.length - 2] = (byte)sum;
      wire[wire.length - 1] = (byte)(sum >> 8);
      offset = 0;
      chunk();
    }
    void chunk() {
      if (done || offset >= wire.length)
        return;
      int end = Math.min(offset + 20, wire.length);
      write.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
      write.setValue(Arrays.copyOfRange(wire, offset, end));
      offset = end;
      if (!g.writeCharacteristic(write))
        finish("write refused");
    }
    public void onCharacteristicWrite(BluetoothGatt g,
                                      BluetoothGattCharacteristic c,
                                      int status) {
      if (status != 0) {
        finish("write=" + status);
        return;
      }
      h.postDelayed(() -> chunk(), 50);
    }
    public void onCharacteristicChanged(BluetoothGatt g,
                                        BluetoothGattCharacteristic c) {
      byte[] v = c.getValue();
      byte[] b = Arrays.copyOf(buffer, buffer.length + v.length);
      System.arraycopy(v, 0, b, buffer.length, v.length);
      buffer = b;
      while (buffer.length >= 3) {
        if (buffer[0] != 1) {
          buffer = Arrays.copyOfRange(buffer, 1, buffer.length);
          continue;
        }
        int n = (buffer[1] & 255) | ((buffer[2] & 255) << 8);
        n += 4;
        if (n > 2048) {
          finish("oversized reply");
          return;
        }
        if (buffer.length < n)
          return;
        byte[] f = Arrays.copyOf(buffer, n);
        buffer = Arrays.copyOfRange(buffer, n, buffer.length);
        int sum = 0;
        for (int k = 1; k < n - 3; k++)
          sum += f[k] & 255;
        if (n < 9 || f[n - 1] != 2 ||
            ((f[n - 3] & 255) | ((f[n - 2] & 255) << 8)) != (sum & 65535)) {
          finish("bad reply");
          return;
        }
        reply(f);
      }
    }
    void trace(int cursor) {
      send(new byte[] {0x37,0x7f,0x44,0x4c,0x55,0x41,13,
                      (byte)cursor,(byte)(cursor>>8),(byte)(cursor>>16),(byte)(cursor>>24)},++sequence);
    }
    static int u32(byte[] f, int at) {
      return (f[at]&255)|((f[at+1]&255)<<8)|((f[at+2]&255)<<16)|((f[at+3]&255)<<24);
    }
    void maintenance() {
      // Temporary RAM-only app; the saved remote and its settings are untouched.
      byte[] source = ("local done=false;return {init=function() keyboard.disconnect() end,"
          + "update=function() if not done and keyboard.status().state==0 then "
          + "keyboard.mode('combined');done=true end end}")
          .getBytes(java.nio.charset.StandardCharsets.UTF_8);
      byte[] payload;
      switch (maintenanceStep++) {
        case 0: payload = new byte[] {2}; break;
        case 1: payload = new byte[] {3,(byte)source.length,(byte)(source.length>>8),1}; break;
        case 2:
          payload = new byte[3+source.length];payload[0]=4;
          System.arraycopy(source,0,payload,3,source.length);break;
        case 3: payload = new byte[] {5}; break;
        default: finish("temporary maintenance app started; saved app unchanged");return;
      }
      byte[] p = new byte[6+payload.length];
      byte[] head = {0x37,0x7f,0x44,0x4c,0x55,0x41};
      System.arraycopy(head,0,p,0,6);System.arraycopy(payload,0,p,6,payload.length);
      send(p,++sequence);
    }
    void reply(byte[] f) {
      int cmd = f[4] & 255;
      if (f[5] != 0x55) {
        finish("negative reply");
        return;
      }
      if (stage == 4 && cmd == 0x37 && f.length>=25 && f[6]=='D' && f[7]=='B' && f[8]=='T' && f[9]=='R') {
        int count=f[12]&255, cursor=0;
        if (f[11]!=0 || count>6 || f.length!=25+24*count) {finish("bad trace");return;}
        for (int i=0;i<count;i++) {
          int at=22+24*i;cursor=u32(f,at);
          StringBuilder hex=new StringBuilder();
          for (int k=0;k<24;k++) hex.append(String.format(java.util.Locale.ROOT,"%02x",f[at+k]&255));
          Log.i("DitooTVControl","trace="+hex);
        }
        if (count==0 || cursor>=u32(f,18)) finish("trace complete");
        else {final int next=cursor;h.postDelayed(() -> trace(next),80);}
        return;
      }
      if (stage == 1 && cmd == 0x33) {
        stage = 2;
        h.postDelayed(() -> send(new byte[] {0x37, 0}, 0x0102), 80);
      } else if (stage == 2 && cmd == 0x37 && f.length == 14 && f[6] == 1) {
        int version = (f[7] & 255) | ((f[8] & 255) << 8) |
                      ((f[9] & 255) << 16) | ((f[10] & 255) << 24);
        Log.i("DitooTVControl", "firmware=" + version);
        if (version != 306035 && version != 306036) {
          finish("wrong firmware=" + version);
          return;
        }
        if ("@trace".equals(message)) {
          stage = 4;
          h.postDelayed(() -> trace(0), 80);
          return;
        }
        if ("@maintenance".equals(message)) {
          stage = 5;
          h.postDelayed(() -> maintenance(), 80);
          return;
        }
        stage = 3;
        byte[] msg =
            message == null
                ? new byte[0]
                : message.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        byte[] p = new byte[7 + msg.length];
        byte[] head = {0x37,
                       0x7f,
                       0x44,
                       0x4c,
                       0x55,
                       0x41,
                       (byte)(message == null ? 0 : 8)};
        System.arraycopy(head, 0, p, 0, 7);
        System.arraycopy(msg, 0, p, 7, msg.length);
        h.postDelayed(() -> send(p, 0x0103), 80);
      } else if (stage == 5 && cmd == 0x37 && f.length >= 49 && f[6]=='D' && f[7]=='L' && f[8]=='U' && f[9]=='A') {
        if (f[12]!=0) {finish("maintenance upload error="+(f[12]&255));return;}
        h.postDelayed(() -> maintenance(), 300);
      } else if (stage == 3 && cmd == 0x37 && f.length >= 49 && f[6] == 'D' &&
                 f[7] == 'L' && f[8] == 'U' && f[9] == 'A') {
        int size = f[13] & 255;
        String result = new String(f, 46, Math.min(size, f.length - 49),
                                   java.nio.charset.StandardCharsets.UTF_8);
        finish("message=" + message + " state=" + (f[11] & 255) +
               " error=" + (f[12] & 255) + " result=" + result);
      }
    }
  }
}
