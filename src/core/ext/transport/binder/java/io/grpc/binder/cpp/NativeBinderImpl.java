package io.grpc.binder.cpp;

// import android.os.Binder.IBinder.FLAG_ONEWAY;

import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;
import java.util.ArrayList;
import java.util.List;

final class NativeBinderImpl {
  // From grpc-java
  static final class LeakSafeOneWayBinder extends Binder {
    public LeakSafeOneWayBinder() {}

    @Override
    protected boolean onTransact(int code, Parcel parcel, Parcel reply, int flags) {
      try {
        onTransaction(this, code, parcel);
        return true;
      } catch (RuntimeException re) {
        Log.w("NativeBinderImpl", "failure sending transaction " + code + re);
        return false;
      }
    }

    @Override
    public boolean pingBinder() {
      return true;
    }
  }

  // static Map<String, LeakSafeOneWayBinder> s = new HashMap<>();
  static List<LeakSafeOneWayBinder> binders = new ArrayList<>();

  static IBinder AIBinderNew() {
    LeakSafeOneWayBinder b = new LeakSafeOneWayBinder();
    binders.add(b);
    return b;
  }

  static List<Parcel> parcels = new ArrayList<>();

  static Parcel AIBinderPrepareTransaction(IBinder b) {
    // Note that after Android Tiramisu the `public static Parcel obtain (IBinder binder)` overload
    // is prefered.
    Parcel p = Parcel.obtain();
    parcels.add(p);
    return p;
  }

  /** Returns True if the call is successfull. */
  static Boolean AIBinderTransact(IBinder b, int code, Parcel in, int flags) {
    // Other flag combination is not supported.
    if (flags != Binder.FLAG_ONEWAY) {
      Log.w("NativeBinderImpl", "Unsupported flags " + flags);
      return false;
    }
    try {
      return b.transact(code, in, null, flags);
    } catch (RemoteException re) {
      Log.w("NativeBinderImpl", "failure sending transaction " + re);
      return false;
    }
  }

  static int AParcelGetDataSize(Parcel parcel) {
    return parcel.dataSize();
  }

  static void AParcelWriteInt32(Parcel parcel, int i) {
    parcel.writeInt(i);
  }

  static void AParcelWriteInt64(Parcel parcel, long l) {
    parcel.writeLong(l);
  }

  static void AParcelWriteStrongBinder(Parcel parcel, IBinder binder) {
    parcel.writeStrongBinder(binder);
  }

  static void AParcelWriteString(Parcel parcel, String string) {
    parcel.writeString(string);
  }

  static int AParcelReadInt32(Parcel parcel) {
    return parcel.readInt();
  }

  static long AParcelReadInt64(Parcel parcel) {
    return parcel.readLong();
  }

  static IBinder AParcelReadStrongBinder(Parcel parcel) {
    return parcel.readStrongBinder();
  }

  static String AParcelReadString(Parcel parcel) {
    return parcel.readString();
  }

  static void AParcelWriteByteArray(Parcel parcel, byte[] byteArray) {
    parcel.writeByteArray(byteArray);
  }

  static byte[] AParcelReadByteArray(Parcel parcel) {
    return parcel.createByteArray();
  }

  // TODO: change return type to binder_status_t and handle errors?
  public static native void onTransaction(Binder binder, int txCode, Parcel in);
}
