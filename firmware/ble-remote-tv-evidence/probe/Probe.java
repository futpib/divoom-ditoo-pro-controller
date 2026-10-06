package dev.ditoo.tvprobe;
import android.app.Activity;
import android.bluetooth.*;
import android.os.Bundle;
import android.util.Log;
public class Probe extends Activity {
  public void onCreate(Bundle b) {
    super.onCreate(b);
    try {
      BluetoothAdapter a =
          getSystemService(BluetoothManager.class).getAdapter();
      BluetoothDevice d = a.getRemoteDevice("B1:21:81:DD:B8:9B");
      String action = getIntent().getStringExtra("action");
      Log.i("DitooTVProbe",
            "before type=" + d.getType() + " bond=" + d.getBondState());
      if ("refresh".equals(action)) {
        d.connectGatt(
            getApplicationContext(), false, new BluetoothGattCallback() {
              public void onConnectionStateChange(BluetoothGatt g, int status,
                                                  int state) {
                try {
                  if (status == 0 && state == 2)
                    Log.i(
                        "DitooTVProbe",
                        "refresh=" +
                            BluetoothGatt.class.getMethod("refresh").invoke(g));
                } catch (Throwable t) {
                  Log.e("DitooTVProbe", "refresh failed", t);
                } finally {
                  g.close();
                }
              }
            }, BluetoothDevice.TRANSPORT_LE);
      }
      if ("pair-le".equals(action)) {
        a.cancelDiscovery();
        Log.i("DitooTVProbe",
              "requested=" +
                  BluetoothDevice.class.getMethod("createBond", int.class)
                      .invoke(d, 2));
      }
    } catch (Throwable t) {
      Log.e("DitooTVProbe", "failed", t);
    }
    finish();
  }
}
