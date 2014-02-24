Getting Started
******************************

Building PJSUA2
======================
The PJSUA2 C++ library will be built by default by PJSIP build system.

Building Python and Java SWIG Modules
======================================
The SWIG modules for Python and Java are built by invoking ``make`` and ``make install`` manually from ``pjsip-apps/src/swig`` directory. The ``make install`` will install the Python SWIG module to user's ``site-packages`` directory.

Requirements
------------

#. ``JDK``.
#. ``Python``, version 2.7 or above is required.
   For **Linux/UNIX**, you will also need ``Python developent package`` (called ``python-devel`` (e.g. on Fedora) or ``python2.7-dev`` (e.g. on Ubuntu)). For **Windows**, you will need MinGW and ``Python SDK`` such as `ActivePython-2.7.5`_ from `ActiveState`_.

.. _`ActivePython-2.7.5`: http://www.activestate.com/activepython/downloads
.. _`ActiveState`: http://www.activestate.com

Testing The Installation
------------------------
To test the installation, simply run python and import ``pjsua2`` module::

  $ python
  > import pjsua2
  > ^Z


Using in C++ Application
========================
As mentioned in previous chapter, a C++ application can use *pjsua2* natively, while at the same time still has access to the lower level objects and the ability to extend the libraries if it needs to. Using the API will be exactly the same as the API reference that is written in this book.

Here is a sample complete C++ application to give you some idea about the API. The snippet below initializes the library and creates an account that registers to our pjsip.org SIP server.

.. code-block:: c++
    
  #include <pjsua2.hpp>
  #include <iostream>
  
  using namespace pj;
  
  // Subclass to extend the Account and get notifications etc.
  class MyAccount : public Account {
  public:
      virtual void onRegState(OnRegStateParam &prm) {
          AccountInfo ai = getInfo();
          std::cout << (ai.regIsActive? "*** Register:" : "*** Unregister:")
                    << " code=" << prm.code << std::endl;
      }
  };

  int main()
  {
      Endpoint ep;
      
      ep.libCreate();
      
      // Initialize endpoint
      EpConfig ep_cfg;
      ep.libInit( ep_cfg );
      
      // Create SIP transport. Error handling sample is shown
      TransportConfig tcfg;
      tcfg.port = 5060;
      try {
          ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
      } catch (Error &err) {
          std::cout << err.info() << std::endl;
          return 1;
      }
      
      // Start the library (worker threads etc)
      ep.libStart();
      std::cout << "*** PJSUA2 STARTED ***" << std::endl;
      
      // Configure an AccountConfig
      AccountConfig acfg;
      acfg.idUri = "sip:test@pjsip.org";
      acfg.regConfig.registrarUri = "sip:pjsip.org";
      AuthCredInfo cred("digest", "*", "test", 0, "secret");
      acfg.sipConfig.authCreds.push_back( cred );
      
      // Create the account
      MyAccount *acc = new MyAccount;
      acc->create(acfg);
      
      // Here we don't have anything else to do..
      pj_thread_sleep(10000);
      
      // Delete the account. This will unregister from server
      delete acc;
      
      // This will implicitly shutdown the library
      return 0;
  }


Using in Python Application
===========================
The equivalence of the C++ sample code above in Python is as follows:

.. code-block:: python

  # Subclass to extend the Account and get notifications etc.
  class Account(pj.Account):
    def onRegState(self, prm):
        print "***OnRegState: " + prm.reason

  # pjsua2 test function
  def pjsua2_test():
    # Create and initialize the library
    ep_cfg = pj.EpConfig()
    ep = pj.Endpoint()
    ep.libCreate()
    ep.libInit(ep_cfg)
    
    # Create SIP transport. Error handling sample is shown
    sipTpConfig = pj.TransportConfig();
    sipTpConfig.port = 5060;
    ep.transportCreate(pj.PJSIP_TRANSPORT_UDP, sipTpConfig);
    # Start the library
    ep.libStart();
    
    acfg = pj.AccountConfig();
    acfg.idUri = "sip:test@pjsip.org";
    acfg.regConfig.registrarUri = "sip:pjsip.org";
    cred = pj.AuthCredInfo("digest", "*", "test", 0, "pwtest");
    acfg.sipConfig.authCreds.append( cred );
    # Create the account
    acc = Account();
    acc.create(acfg);
    # Here we don't have anything else to do..
    time.sleep(10);

    # Destroy the library
    ep.libDestroy()

  #
  # main()
  #
  if __name__ == "__main__":
    pjsua2_test()


Using in Java Application
=========================
The equivalence of the C++ sample code above in Java is as follows:

.. code-block:: java

  import org.pjsip.pjsua2.*;

  // Subclass to extend the Account and get notifications etc.
  class MyAccount extends Account {
    @Override
    public void onRegState(OnRegStateParam prm) {
        System.out.println("*** On registration state: " + prm.getCode() + prm.getReason());
    }
  }

  public class test {
    static {
        System.loadLibrary("pjsua2");
        System.out.println("Library loaded");
    }
    
    public static void main(String argv[]) {
        try {
            // Create endpoint
            Endpoint ep = new Endpoint();
            ep.libCreate();
            // Initialize endpoint
            EpConfig epConfig = new EpConfig();
            ep.libInit( epConfig );
            // Create SIP transport. Error handling sample is shown
            TransportConfig sipTpConfig = new TransportConfig();
            sipTpConfig.setPort(5060);
            ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP, sipTpConfig);
            // Start the library
            ep.libStart();

            AccountConfig acfg = new AccountConfig();
            acfg.setIdUri("sip:test@pjsip.org");
            acfg.getRegConfig().setRegistrarUri("sip:pjsip.org");
            AuthCredInfo cred = new AuthCredInfo("digest", "*", "test", 0, "secret");
            acfg.getSipConfig().getAuthCreds().add( cred );
            // Create the account
            MyAccount acc = new MyAccount();
            acc.create(acfg);
            // Here we don't have anything else to do..
            Thread.sleep(10000);
            /* Explicitly delete the account.
             * This is to avoid GC to delete the endpoint first before deleting
             * the account.
             */
            acc.delete();
            
            // Explicitly destroy and delete endpoint
            ep.libDestroy();
            ep.delete();
            
        } catch (Exception e) {
            System.out.println(e);
            return;
        }
    }
  }


