package io.grpc.binder.cpp;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;

/** Connects to a service synchronously */
public class SyncServiceConnection implements ServiceConnection {
  private final String logTag = "SyncServiceConnection";

  private Context mContext;
  private IBinder mService;

  public SyncServiceConnection(Context context) {
    mContext = context;
  }

  @Override
  public void onServiceConnected(ComponentName className, IBinder service) {
    Log.e(logTag, "Service has connected: ");
    synchronized (this) {
      mService = service;
    }
  }

  @Override
  public void onServiceDisconnected(ComponentName className) {
    Log.e(logTag, "Service has disconnected: ");
  }

  public void tryConnect() {
    synchronized (this) {
      Intent intent = new Intent("grpc.io.action.BIND");
      // TODO(mingcl): The component name is currently hard-coded here. But we should pump the
      // component name from C++ to here instead
      ComponentName compName =
          new ComponentName(
              "com.google.apps.tiktok.helloworld.ondeviceserver.server",
              "com.google.apps.tiktok.helloworld.ondeviceserver.server.ExportedEndpointService");
      intent.setComponent(compName);
      // Will return true if the system is in the process of bringing up a service that your client
      // has
      // permission to bind to; false if the system couldn't find the service or if your client
      // doesn't have permission to bind to it
      boolean result = mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);
      if (result) {
        Log.e(logTag, "bindService ok");
      } else {
        Log.e(logTag, "bindService not ok");
      }
    }
  }

  public IBinder getIBinder() {
    return mService;
  }
}
