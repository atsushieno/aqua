#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "aria2web.h"

void sample_cc_callback(aria2web* context, int cc, int value)
{
	printf("sample CC callback: CC#%x = %d\n", cc, value);
}

void sample_note_callback(aria2web* context, int key, int velocity)
{
	printf("sample Note callback: key %d velocity %d\n", key, velocity);
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
	auto a2w = aria2web_create();
	aria2web_set_control_change_callback(a2w, sample_cc_callback, nullptr);
	aria2web_set_note_callback(a2w, sample_note_callback, nullptr);
	aria2web_start(a2w);
	puts("Hit [CR] to stop");
	getchar();
	puts("stopped.");
	aria2web_stop(a2w);
	aria2web_free(a2w);
}

