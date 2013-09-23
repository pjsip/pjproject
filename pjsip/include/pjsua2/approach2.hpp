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
public:
    virtual ~Account();

    void setRegistration(bool renew=true);

protected:
    friend class EndpointCallback;

    /* Users are not supposed to instantiate this directly. They either need
     * to subclasss this, or create from endpoint.
     */
    Account(const AccountConfig &cfg, AccountCallback &cb);
};


class EndpointCallback
{
public:
    /* By default this creates Account instance, but if the app subclasses
     * Account, it can implement this callback.
     */
    virtual Account *onCreateAccount(const AccountConfig &cfg,
                                     AccountCallback &cb)
    {
	return new Account(cfg, cb);
    }

    /* Other callbacks: ... */
    virtual void onTransportStateChanged(
			const TransportStateChangedParam &prm)
    {}

};

class Endpoint
{
public:
    Endpoint(EndpointCallback *cb);

    /* This is the only way to create account */
    Account* createAccount(const AccountConfig &cfg, AccountCallback &cb)
    {
	Account *acc = this->endpointCallback->onCreateAccount(cfg, cb);
	acc->setRegistration();
	return acc;
    }
};

///////////////////////////// Sample Application /////////////////////////////
class MyAccountCallback : public AccountCallback
{
public:
    virtual void onRegState(RegStateParam &prm)
    { /* do stuff */  }
};

class MyAccount: public Account
{
public:
    MyAccount(const AccountConfig &cfg, AccountCallback &cb);

    void doNewThing();
};

class MyEndpointCallback : public EndpointCallback
{
public:
    virtual Account *onCreateAccount(const AccountConfig &cfg, AccountCallback &cb)
    {
	return new MyAccount(cfg, cb);
    };
};


int main()
{
    Endpoint ep(new MyEndpointCallback);

    Account *acc = ep.createAccount(accCfg, new MyAccountCallback);
    acc->setRegistration(true);
};


