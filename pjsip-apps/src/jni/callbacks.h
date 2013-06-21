#include <pjsua-lib/pjsua.h>

class PjsuaCallback {
public:
    virtual void on_call_media_state (pjsua_call_id call_id) {}
    virtual ~PjsuaCallback() {}
};

extern "C" {
void setPjsuaCallback(PjsuaCallback* callback);
}
