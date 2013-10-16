public class test {
  static {
    System.loadLibrary("pjsua2");
    System.out.println("Library loaded");
  }

  public static void main(String argv[]) {

	AuthCredInfo cred = new AuthCredInfo();

	cred.setRealm("Hello world");
    
	System.out.println(cred.getRealm());
  }
}
