#define HTTPSERVER_IMPL
#define AQUA_IMPL
#include "aqua.h"

bool standalone{true};

/*
  The standalone app is used for debugging as well as the UI remote process for LV2 UI.

  With --plugin argument, it runs as a plugin UI and prints 'C' for CC or 'N' for note on/off
  using hexadecimal representation prefixed as in "#xx,#xx\n" format.
  It must call fflush(stdout) every time so that the plugin UI module can process an entire line.
 */

void initialized_callback(void* context)
{
	if (standalone)
		printf("'Initizlied' callback\n");
	else {
		printf("I\n");
		fflush(stdout);
	}
}

void control_change_callback(void* context, int cc, int value)
{
	if (standalone)
		printf("Control change callback: CC#%x = %d\n", cc, value);
	else {
		printf("C#%x,#%x\n", cc, value);
		fflush(stdout);
	}
}

void note_callback(void* context, int key, int velocity)
{
	if (standalone)
		printf("Note callback: key %d velocity %d\n", key, velocity);
	else {
		printf("N#%x,#%x\n", key, velocity);
		fflush(stdout);
	}
}

void change_program_callback(void* context, const char* sfzFilename)
{
	if (standalone)
		printf("Change program callback: %s\n", sfzFilename);
	else {
		printf("P %s\n", sfzFilename);
		fflush(stdout);
	}
}

void* launch_aqua(void* context) {
	aqua_start((aqua*) context);
	return nullptr;
};

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main(int argc, char** argv) {
#endif

	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "--plugin") == 0)
			standalone = false;

	char* rpath = realpath(argv[0], nullptr);
	*strrchr(rpath, '/') = '\0';
	std::string path{rpath};
	free(rpath);
	path = path + "/web";

	printf("#aqua-host: started. Web directory is %s \n", path.c_str());
	fflush(stdout);

	auto aqua = aqua_create(path.c_str());
	aqua_set_initialized_callback(aqua, initialized_callback, nullptr);
	aqua_set_control_change_callback(aqua, control_change_callback, nullptr);
	aqua_set_note_callback(aqua, note_callback, nullptr);
	aqua_set_change_program_callback(aqua, change_program_callback, nullptr);

	pthread_t aqua_thread;
	pthread_create(&aqua_thread, nullptr, launch_aqua, aqua);
	while (!aqua->webview_ready)
		usleep(1000);
	if (standalone)
		puts("Type \"quit\" (all in lowercase) to stop");
	char input[1024];
	while (1) {
		if (!fgets(input, 1023, stdin)) {
			puts("#aqua-host: error - could not read input. aborting.");
			break;
		}
		int len = strlen(input);
		if (len > 0 && input[len - 1] == '\n')
			input[--len] = '\0';
		if (len > 0 && input[len - 1] == '\r')
			input[--len] = '\0';
		if (strcmp(input, "quit") == 0)
			break;
		else if (strcmp(input, "hide") == 0)
			aqua_hide_window(aqua);
		else if (strcmp(input, "show") == 0)
			aqua_show_window(aqua);
		else if (strncmp(input, "SFZ ", 4) == 0)
			aqua_load_sfz(aqua, input + 4);
		else
			printf("#aqua-host: unknown user input (after removing CR/LF): %s\n", input);
	}
	puts("#aqua-host: stopped");
	aqua_stop(aqua);
	aqua_free(aqua);
}
