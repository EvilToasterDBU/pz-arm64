public class LightingTest2 {
    public static void main(String[] a) throws Exception {
        zombie.iso.LightingJNI.init();
        System.out.println("init ok: " + zombie.iso.LightingJNI.init);
        System.out.println("calling stateBeginUpdate...");
        zombie.iso.LightingJNI.stateBeginUpdate(0, 0, 0, 0, 0);
        System.out.println("calling stateEndUpdate...");
        zombie.iso.LightingJNI.stateEndUpdate();
        System.out.println("stateEndUpdate returned OK");
    }
}
