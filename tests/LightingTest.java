public class LightingTest {
    public static void main(String[] a) throws Exception {
        System.out.println("before LightingJNI.init()");
        zombie.iso.LightingJNI.init();
        System.out.println("after init: LightingJNI.init flag = " + zombie.iso.LightingJNI.init);
    }
}
