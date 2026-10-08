import java.lang.reflect.Method;
public class BrowserTest {
    static Object call(Class<?> c, String name, Class<?>[] sig, Object... args) throws Exception {
        Method m = c.getDeclaredMethod(name, sig); m.setAccessible(true); return m.invoke(null, args);
    }
    public static void main(String[] a) throws Exception {
        System.loadLibrary("RakNet64");
        System.loadLibrary("ZNetJNI64");
        Class<?> zn = Class.forName("zombie.core.znet.ZNet");
        call(zn, "init", new Class[]{});
        System.out.println("ZNet.init done");
        Class<?> su = Class.forName("zombie.core.znet.SteamUtils");
        Class<?> sb = Class.forName("zombie.core.znet.ServerBrowser");
        System.out.println("SteamUtils.n_Init -> " + call(su, "n_Init", new Class[]{boolean.class}, false));
        System.out.println("ServerBrowser.n_Init -> " + call(sb, "n_Init", new Class[]{}));
        call(sb, "n_RefreshInternetServers", new Class[]{});
        System.out.println("refresh requested; waiting for replies...");
        long end = System.currentTimeMillis() + 25000;
        int last = -1;
        while (System.currentTimeMillis() < end) {
            call(su, "n_RunLoop", new Class[]{});
            int n = (Integer) call(sb, "n_GetServerCount", new Class[]{});
            boolean ref = (Boolean) call(sb, "n_IsRefreshing", new Class[]{});
            if (n != last) { System.out.println("servers=" + n + " refreshing=" + ref); last = n; }
            Thread.sleep(250);
        }
        int n = (Integer) call(sb, "n_GetServerCount", new Class[]{});
        for (int i = 0; i < Math.min(n, 3); i++) {
            Object d = call(sb, "n_GetServerDetails", new Class[]{int.class}, i);
            System.out.println("details[" + i + "] = " + d);
        }
        System.out.println("DONE servers=" + n);
    }
}
