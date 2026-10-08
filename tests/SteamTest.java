import java.lang.reflect.Method;
public class SteamTest {
    public static void main(String[] a) throws Exception {
        System.loadLibrary("RakNet64");
        System.loadLibrary("ZNetJNI64");
        System.out.println("ZNetJNI64 shim loaded");
        Class<?> c = Class.forName("zombie.core.znet.SteamUtils");
        Method m = c.getDeclaredMethod("n_Init", boolean.class);
        m.setAccessible(true);
        Object r = m.invoke(null, false);
        System.out.println("SteamUtils.n_Init() -> " + r);
        Method run = c.getDeclaredMethod("n_RunLoop");
        run.setAccessible(true);
        run.invoke(null);
        System.out.println("n_RunLoop ok");
    }
}
