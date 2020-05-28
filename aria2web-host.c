#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "aria2web.h"

bool standalone{TRUE};

/*
  The standalone app is only for debugging.

  With --plugin argument, it runs as a plugin UI and prints 'C' for CC or 'N' for note on/off
  using hexadecimal representation prefixed as in "#xx,#xx\n" format.
 */

void sample_cc_callback(aria2web* context, int cc, int value)
{
	if (standalone)
		printf("sample CC callback: CC#%x = %d\n", cc, value);
	else
		printf("C#%x,#%x\n", cc, value);
}

void sample_note_callback(aria2web* context, int key, int velocity)
{
	if (standalone)
		printf("sample Note callback: key %d velocity %d\n", key, velocity);
	else
		printf("N#%x,#%x\n", key, velocity);
}

void sample_window_close_callback(aria2web* context, int key, int velocity)
{
	printf("closing\n");
	aria2web_stop(context);
	exit(0);
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main(int argc, char** argv) {
#endif
	for (int i = 1; i < argc; i++)
		if(strcmp(argv[i], "--plugin") == 0)
			standalone = FALSE;

	auto a2w = aria2web_create();
	aria2web_set_control_change_callback(a2w, sample_cc_callback, nullptr);
	aria2web_set_note_callback(a2w, sample_note_callback, nullptr);
	aria2web_set_window_close_callback(a2w, sample_window_close_callback, nullptr);
	aria2web_start(a2w);
	if (standalone)
		puts("Hit [CR] to stop");
	getchar();
	if (standalone)
		puts("stopped.");
	aria2web_stop(a2w);
	aria2web_free(a2w);
}
