
#ifndef PJSUA_PAN_H
#define PJSUA_PAN_H

/** pjsua application panic codes */
enum TpjsuaPanics
	{
	EpjsuaUi = 1
	// add further panics here
	};

inline void Panic(TpjsuaPanics aReason)
	{
	_LIT(applicationName,"pjsua");
	User::Panic(applicationName, aReason);
	}

#endif // PJSUA_PAN_H
