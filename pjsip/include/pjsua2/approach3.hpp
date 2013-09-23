//////////////////////////////// API ////////////////////////////////

class AccountCallback
{
public:
    Account *getAccount();

    virtual ~AccountCallback() {}

    virtual void onRegState(RegStateParam &prm);

protected:
    void setAccount(Account *acc);

private:
    Account *acc;
};

class Account
{
public: /* Operations */
    virtual ~Account();

    void create();
    void setRegistration(bool renew=true);

protected:
    /* App should always subclass this. */
    Account(Endpoint &ep, const AccountConfig &cfg);
};


class EndpointCallback
{
public:

    /* Other callbacks: ... */
    virtual void onTransportStateChanged(
			const TransportStateChangedParam &prm)
    {}

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
    MyAccount acc(ep, accCfg);
    acc.setRegistration(true);
};
