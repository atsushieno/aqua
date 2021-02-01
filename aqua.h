

/* These functions should be part of the public API */

typedef struct Aqua_tag Aqua;
/* The value and velocity could be float, but webaudio-controls seems to handle them in integer. */
typedef void(* aqua_initialized_callback)(void* context);
typedef void(* aqua_control_change_callback)(void* context, int cc, int value);
typedef void(* aqua_note_callback)(void* context, int key, int velocity);
typedef void(* aqua_change_program_callback)(void* context, const char* sfzFilename);

Aqua* aqua_create(const char* webLocalFilePath);
void aqua_start(Aqua* context, void* parentWindow = nullptr);
void aqua_set_initialized_callback(Aqua* aqua, aqua_initialized_callback callback, void* context);
void aqua_set_control_change_callback(Aqua* aqua, aqua_control_change_callback callback, void* context);
void aqua_set_note_callback(Aqua* aqua, aqua_note_callback callback, void* context);
void aqua_set_change_program_callback(Aqua* aqua, aqua_change_program_callback callback, void* context);
void aqua_stop(Aqua* context);
void* aqua_get_native_widget(Aqua* instance);

/* The rest of the code is private */

#ifdef AQUA_IMPL

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string>
#include <vector>

#include <webview.h>
#include <httpserver.h>

void* aqua_run_http_server(void* context);
void* aqua_run_webview_loop(void* context);

void log_debug(const char* s) { puts(s); }

static int confirmed_port = -1;

int aqua_get_http_server_port_number()
{
	if (confirmed_port > 0)
		return confirmed_port;

	int port = 37564;
	while(1) {
		/* allocate it dynamically, if default port is not available. Note that we might instiantiate it more than once. */
		int sfd;
		sfd = socket(AF_INET, SOCK_STREAM, 0);
		assert(sfd != -1);
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_port = port;
		if (bind(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) != -1) {
			close(sfd);
			confirmed_port = port;
			printf("# aqua using port %d\n", port);
			fflush(stdout);
			return port;
		}
		port = 49152 + random() % (0xFFFF - 49152);
    }
}

typedef struct Aqua_tag {
	pthread_t http_server_thread{0};
	std::string web_local_file_path{};
	std::unique_ptr<std::vector<std::string>> local_instrument_files{nullptr};
	std::unique_ptr<std::vector<std::string>> local_instrument_dirs{nullptr};
	void* webview{nullptr};
	void* parent_window{nullptr};
	void* webview_widget{nullptr};
	bool http_server_started{false};
	bool webview_ready{false};
	void* component_ref{nullptr};
	aqua_initialized_callback initialized_callback{nullptr};
	void* initialized_callback_context{nullptr};
	aqua_control_change_callback control_change_callback{nullptr};
	void* cc_callback_context{nullptr};
	aqua_note_callback note_callback{nullptr};
	void* note_callback_context{nullptr};
	aqua_change_program_callback change_program_callback{nullptr};
	void* change_program_callback_context{nullptr};

	void start(void* parentWindow = nullptr)
	{
		pthread_create(&http_server_thread, NULL, aqua_run_http_server, this);
		pthread_setname_np(http_server_thread, "aqua_httpserver");
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = 1000;
		while (!http_server_started)
			usleep(1000);
		parent_window = parentWindow;
		aqua_run_webview_loop(this);
	}

	void stop()
	{
		webview_destroy(webview);
		pthread_cancel(http_server_thread);
	}

} aqua;

Aqua* aqua_create(const char* webLocalFilePath) {
	auto ret = new aqua(); 
	ret->web_local_file_path = strdup(webLocalFilePath);
	return ret;
}

void aqua_free(Aqua* instance) {
	delete instance;
}

void aqua_start(Aqua* instance, void* parentWindow) { instance->start(parentWindow); }

void aqua_stop(Aqua* instance) { instance->stop(); }

void* aqua_get_native_widget(Aqua* instance) {
	return instance->webview_widget;
}

void uri_unescape_in_place(char* p) {
	int dst = 0;
	for (int i = 0; p[i]; i++) {
		if (p[i] == '%') {
			if (p[i + 1] && p[i + 2]) {
				p[dst++] = (p[i + 1] - 0x30) * 16 + p[i + 2] - 0x30;
				i += 2;
			}
			else
				p[dst++] = '%';
		}
		else
			p[dst++] = p[i];
	}
	p[dst] = '\0';
}

void uri_strip_query_string(char *s) {
	for (int i = 0; s[i]; i++)
		if (s[i] == '?') {
			s[i] = '\0';
			return;
		}
}

const char* mime_types[] = {"text/html", "application/xml", "image/png" };

const char* get_mime_type_from_filename(char* filename) {
	std::string s{filename};
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".xml") == 0)
		return mime_types[1];
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".png") == 0)
		return mime_types[2];
	return mime_types[0];
}

/*
  The HTTP request mapping is super hacky; if the requested URLs begin with any of
  the local instrument file directories, then it just returns the file content from
  local files.
  Keep in mind that this is a local application which is not as secure as web pages.
 */
void handle_request(struct http_request_s* request) {
	auto aqua = (Aqua*) http_request_server_userdata(request);

	struct http_response_s* response = http_response_init();

	struct http_string_s target = http_request_target(request);
	char* targetPath = (char*) calloc(target.len + 2, 1);

	targetPath[0] = '.';
	memcpy(targetPath + 1, target.buf, target.len);
	targetPath[target.len + 1] = '\0';
	uri_unescape_in_place(targetPath);
	uri_strip_query_string(targetPath);

	const char* mimeType = get_mime_type_from_filename(targetPath);

	std::string filePath{};
	for (auto dir : *aqua->local_instrument_dirs) {
		if (strncmp(dir.c_str(), targetPath + 1, dir.length()) == 0) {
			filePath = std::string{targetPath + 1};
			break;
		}
	}
	if (filePath.length() == 0)
		filePath = aqua->web_local_file_path.length() > 0 ? aqua->web_local_file_path + "/" + targetPath : targetPath;

	int fd = open(filePath.c_str(), O_RDONLY);
	free(targetPath);
	if (fd == -1) {
		http_response_status(response, 404);
		http_respond(request, response);
	} else {
		struct stat st;
		fstat(fd, &st);
		void* content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		assert(content);
		
		http_response_status(response, 200);
		http_response_header(response, "Content-Type", mimeType);
		http_response_body(response, (const char*) content, st.st_size);
		http_respond(request, response);
		munmap(content, st.st_size);
	}
}

void* aqua_run_http_server(void* context) {
	auto aqua = (Aqua*) context;
	struct http_server_s* server = http_server_init(aqua_get_http_server_port_number(), handle_request);
	http_server_set_userdata(server, aqua);
	aqua->http_server_started = TRUE;
	http_server_listen(server);
	return nullptr;
}

void aqua_show_window(Aqua* aqua)
{
}

void aqua_hide_window(Aqua* aqua)
{
	gtk_widget_hide((GtkWidget*) webview_get_window((webview_t) aqua->webview));
}

typedef struct {
	Aqua *aqua;
	const char* sfzfile;
} load_sfz_ctx;

int do_load_sfz(void* context) {
	auto ctx = (load_sfz_ctx*) context;
	auto js_log = std::string{} + "console.log('aqua.h: loading UI via SFZ by host request: " + ctx->sfzfile + "');";
	webview_eval((webview_t) ctx->aqua->webview, js_log.c_str());
	auto js = std::string{} + "loadInstrumentFromSfz('" + ctx->sfzfile + "');";
	webview_eval((webview_t) ctx->aqua->webview, js.c_str());
	free(ctx);
	return false;
}

void aqua_load_sfz(Aqua* aqua, const char* sfzfile)
{
	auto ctx = new load_sfz_ctx{aqua, sfzfile};
	g_idle_add(do_load_sfz, ctx);
}

void aqua_set_initialized_callback(Aqua* aqua, aqua_initialized_callback callback, void* callbackContext)
{
	aqua->initialized_callback = callback;
	aqua->initialized_callback_context = callbackContext;
}

void aqua_set_control_change_callback(Aqua* aqua, aqua_control_change_callback callback, void* callbackContext)
{
	aqua->control_change_callback = callback;
	aqua->cc_callback_context = callbackContext;
}

void aqua_set_note_callback(Aqua* aqua, aqua_note_callback callback, void* callbackContext)
{
	aqua->note_callback = callback;
	aqua->note_callback_context = callbackContext;
}

void aqua_set_change_program_callback(Aqua* aqua, aqua_change_program_callback callback, void* callbackContext)
{
	aqua->change_program_callback = callback;
	aqua->change_program_callback_context = callbackContext;
}

void parse_js_two_array_items(const char* req, int* ret1, int* ret2)
{
	assert(req[0] == '[');
	int end = strlen(req);
	assert(req[end - 1] == ']');
	const char* ptr = strchr(req, ',');
	assert(ptr != nullptr);
	int pos = ptr - req;
	assert(pos > 0);
	*ret1 = 0;
	for (int i = 1; i < pos; i++)
		*ret1 = *ret1 * 10 + req[i] - '0';
	*ret2 = 0;
	for (int i = pos + 1; i < end - 1; i++)
		*ret2 = *ret2 * 10 + req[i] - '0';
}

void webview_callback_initialized(const char *seq, const char *req, void *arg) {
	auto aqua = (Aqua*) arg;
	if (aqua && aqua->initialized_callback)
		aqua->initialized_callback(aqua->initialized_callback_context);
	else {
		log_debug("# aqua.h: 'initialized' callback is invoked");
	}
}

void webview_callback_control_change(const char *seq, const char *req, void *arg) {
	int cc = 0, val = 0;
	parse_js_two_array_items(req, &cc, &val);

	auto aqua = (Aqua*) arg;
	if (aqua && aqua->control_change_callback)
		aqua->control_change_callback(aqua->cc_callback_context, cc, val);
	else {
		log_debug("# aqua.h: control change callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void webview_callback_note(const char *seq, const char *req, void *arg) {
	int state = 0, key = 0;
	parse_js_two_array_items(req, &state, &key);

	auto aqua = (Aqua*) arg;
	if (aqua && aqua->note_callback)
		aqua->note_callback(aqua->note_callback_context, key, state ? 127 : 0);
	else {
		log_debug("# aqua.h: note callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void webview_callback_change_program(const char *seq, const char *req, void *arg) {
	auto aqua = (Aqua*) arg;
	// req = ["filename"] so skip first two bytes and trim last two bytes.
	auto s = strdup(req);
	s[strlen(s) - 2] = '\0';

	if (aqua && aqua->control_change_callback)
		aqua->change_program_callback(aqua->cc_callback_context, s + 2);
	else {
		log_debug("# aqua.h: change program callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
	free(s);
}

void webview_callback_get_local_instruments(const char *seq, const char *req, void *arg) {
	auto aqua = (Aqua*) arg;

	std::string fileList{};
	std::string aqua_config_file{getenv("HOME")};
	aqua_config_file += "/.config/aqua.config";
	struct stat cfgStat;
	if (stat(aqua_config_file.c_str(), &cfgStat) == 0) {
		FILE *fp = fopen(aqua_config_file.c_str(), "r");
		if (fp != nullptr) {
			char buf[1024];
			aqua->local_instrument_files->clear();
			aqua->local_instrument_dirs->clear();
			while (!feof(fp)) {
				buf[0] = '\0';
				fgets(&buf[0], 1023, fp);
				buf[strlen(buf) - 1] = '\0';
				if (buf[strlen(buf) - 1] == '\r')
					buf[strlen(buf) - 1] = '\0';
				if (buf[0] == '\0')
					continue; // empty line
				fileList += (fileList.length() == 0 ? "" : ", ");
				fileList += "\"";
				aqua->local_instrument_files->emplace_back(std::string{buf});
				fileList += buf;
				fileList += "\"";

				if (strrchr(buf, '/') != nullptr)
					*strrchr(buf, '/') = '\0';
				aqua->local_instrument_dirs->emplace_back(std::string{buf});
			}
		} else {
			printf("#aqua.h: failed to load config file %s...\n", aqua_config_file.c_str());
			fflush(stdout);
		}
	}
	std::string cfgJS{"Aqua.Config={BankXmlFiles: [" + fileList + "]}; onLocalBankFilesUpdated();"};

	webview_eval(aqua->webview, cfgJS.c_str());
}

void on_dispatch(webview_t w, void* context) {
	// FIXME: add gtk window close callback here.
}

// It used to be implemented to run on a dedicated thread, which still worked as a standalone UI
// but as a plugin which is loaded and run under DAW GUI it violates UI threading principle, so
// it was rewritten to run within the callers (aqua) thread.
void* aqua_run_webview_loop(void* context) {
	const char* urlfmt = "http://localhost:%d/index.html";
	int port = aqua_get_http_server_port_number();
	char* url = strdup(urlfmt);
	sprintf(url, urlfmt, port);

	auto aqua = (Aqua*) context;
	aqua->local_instrument_files.reset(new std::vector<std::string>());
	aqua->local_instrument_dirs.reset(new std::vector<std::string>());
	void* w = aqua->webview = webview_create(true, nullptr);
	webview_set_title(w, "Aqua embedded example");
	webview_set_size(w, 1200, 450, WEBVIEW_HINT_NONE);
	webview_bind(w, "AquaInitializedCallback", webview_callback_initialized, context);
	webview_bind(w, "AquaControlChangeCallback", webview_callback_control_change, context);
	webview_bind(w, "AquaNoteCallback", webview_callback_note, context);
	webview_bind(w, "AquaChangeProgramCallback", webview_callback_change_program, context);
	webview_bind(w, "AquaGetLocalInstrumentsCallback", webview_callback_get_local_instruments, context);

	webview_dispatch(w, on_dispatch, context);
	webview_navigate(w, url);
	gtk_window_set_deletable((GtkWindow*) webview_get_window(w), FALSE);
	free(url);
	aqua->webview_ready = TRUE;
	webview_run(w);
	return nullptr;
}

#endif /* AQUA_IMPL */
