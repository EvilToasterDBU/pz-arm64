public class FmodTest {
    public static void main(String[] a) throws Exception {
        System.loadLibrary("fmodintegration64");
        System.out.println("shim loaded; calling FMOD_System_Create");
        int r = fmod.javafmodJNI.FMOD_System_Create();
        System.out.println("FMOD_System_Create -> " + r + " (0 = FMOD_OK)");
    }
}
