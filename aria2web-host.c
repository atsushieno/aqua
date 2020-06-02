#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "aria2web.h"

bool standalone{TRUE};

/*
  The standalone app is used for debugging as well as the UI remote process for LV2 UI.

  With --plugin argument, it runs as a plugin UI and prints 'C' for CC or 'N' for note on/off
  using hexadecimal representation prefixed as in "#xx,#xx\n" format.
  It must call fflush(stdout) every time so that the plugin UI module can process an entire line.
 */

void sample_cc_callback(void* context, int cc, int value)
{
	if (standalone)
		printf("sample CC callback: CC#%x = %d\n", cc, value);
	else {
		printf("C#%x,#%x\n", cc, value);
		fflush(stdout);
	}
}

void sample_note_callback(void* context, int key, int velocity)
{
	if (standalone)
		printf("sample Note callback: key %d velocity %d\n", key, velocity);
	else {
		printf("N#%x,#%x\n", key, velocity);
		fflush(stdout);
	}
}

void sample_window_close_callback(void* context)
{
	printf("closing\n");
	aria2web_stop((aria2web*) context);
	exit(0);
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main(int argc, char** argv) {
#endif

	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "--plugin") == 0)
			standalone = FALSE;

	char* rpath = realpath(argv[0], nullptr);
	*strrchr(rpath, '/') = '\0';
	std::string path{rpath};
	free(rpath);

	printf("#aria2web-host: started. current directory is %s \n", path.c_str());
	fflush(stdout);

	auto a2w = aria2web_create(path.c_str());
	aria2web_set_control_change_callback(a2w, sample_cc_callback, nullptr);
	aria2web_set_note_callback(a2w, sample_note_callback, nullptr);
	aria2web_set_window_close_callback(a2w, sample_window_close_callback, nullptr);

	pthread_t a2w_thread;
	pthread_create(&a2w_thread, nullptr, aria2web_start, a2w);
	if (standalone)
		puts("Type \"quit\" (all in lowercase) to stop");
	char input[1024];
	while (1) {
		if (!fgets(input, 1023, stdin)) {
			puts("#aria2web-host: error - could not read input. aborting.");
			break;
		}
		if (input[0] != '\0') {
			input[strlen(input) - 1] = '\0';
			if (input[0] == '\r')
				input[strlen(input) - 1] = '\0';
		}
		printf("#USER INPUT: %s\n", input);
		if (strcmp(input, "quit") == 0)
			break;
		else if (strcmp(input, "hide") == 0)
			aria2web_hide_window(a2w);
		else if (strcmp(input, "show") == 0)
			aria2web_show_window(a2w);
	}
	puts("#aria2web-host: stopped");
	aria2web_stop(a2w);
	aria2web_free(a2w);
}
