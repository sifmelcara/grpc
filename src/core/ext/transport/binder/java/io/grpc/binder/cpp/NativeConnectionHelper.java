package io.grpc.binder.cpp;

import android.content.Context;
import android.os.IBinder;
import android.os.Parcel;

/** Provide interfaces for native code to invoke */
final class NativeConnectionHelper {

  static SyncServiceConnection s;

  static void tryEstablishConnection(Context context) {
    s = new SyncServiceConnection(context);
    s.tryConnect();
  }

  static IBinder getServiceBinder() {
    return s.getIBinder();
  }

  static Parcel getEmptyParcel() {
    return Parcel.obtain();
  }
}
