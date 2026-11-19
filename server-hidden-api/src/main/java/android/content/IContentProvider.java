package android.content;

import android.os.Bundle;
import android.os.IInterface;
import android.os.RemoteException;

// android.content.IContentProvider is hidden, this is a fake one to expose the type to the project
public interface IContentProvider extends IInterface {

    Bundle call(AttributionSource attributionSource, String authority, String method, String arg, Bundle extras) throws RemoteException;

    Bundle call(String callingPackage, String attributionTag, String authority, String method, String arg, Bundle extras) throws RemoteException;

    Bundle call(String callingPackage, String authority, String method, String arg, Bundle extras) throws RemoteException;

    Bundle call(String callingPackage, String method, String arg, Bundle extras) throws RemoteException;
}
