/* File: pjsua_app.i */
%module pjsua_app

%{
int initApp();
void deinitApp();
int startPjsua(char *cfgFile);
void setInput(char *s);
char * getMessage();
void finishDisplayMsg();
%}

int initApp();
void deinitApp();
int startPjsua(char *cfgFile);
void setInput(char *s);
char * getMessage();
void finishDisplayMsg();
