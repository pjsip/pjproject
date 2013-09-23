//////////////////////////////// API ////////////////////////////////

class Account
{
public: /* Operations */
    virtual ~Account();

    void create();
    void setRegistration(bool renew=true);

public: /* Callbacks */
    virtual void onRegState(RegStateParam &prm)
    {}

protected:
    /* App should always subclass this. */
    Account(Endpoint &ep, const AccountConfig &cfg);
};


class Endpoint
{
public: /* Operations */
    Endpoint(EndpointCallback *cb);

public:	/* Callbacks */
    virtual void onTransportStateChanged(
			const TransportStateChangedParam &prm)
    {}
};

///////////////////////////// Sample Application /////////////////////////////
class MyAccount: public Account
{
public:
    MyAccount(Endpoint &ep, const AccountConfig &cfg);

    void doNewThing() {}

    virtual void onRegState(RegStateParam &prm)
    { /* do stuff */  }

};

class MyEndpoint : public Endpoint
{
public:
    virtual void onTransportStateChanged(
			const TransportStateChangedParam &prm)
    {}
};


int main()
{
    MyEndpoint ep;

    // Two steps creation: instantiate, and actually create the acc in pjsua
    // (why??)
    MyAccount acc(accCfg);
    acc.create(ep);

    acc.setRegistration(true);
};
