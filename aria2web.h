

/* These functions should be part of the public API */

typedef struct aria2web_tag aria2web;
/* The value and velocity could be float, but webaudio-controls seems to handle them in integer. */
typedef void(* aria2web_initialized_callback)(void* context);
typedef void(* aria2web_control_change_callback)(void* context, int cc, int value);
typedef void(* aria2web_note_callback)(void* context, int key, int velocity);
typedef void(* aria2web_change_program_callback)(void* context, const char* sfzFilename);

aria2web* aria2web_create(const char* webLocalFilePath);
void aria2web_start(aria2web* context, void* parentWindow = nullptr);
void aria2web_set_initialized_callback(aria2web* a2w, aria2web_initialized_callback callback, void* context);
void aria2web_set_control_change_callback(aria2web* a2w, aria2web_control_change_callback callback, void* context);
void aria2web_set_note_callback(aria2web* a2w, aria2web_note_callback callback, void* context);
void aria2web_set_change_program_callback(aria2web* a2w, aria2web_change_program_callback callback, void* context);
void aria2web_stop(aria2web* context);
void* aria2web_get_native_widget(aria2web* instance);

/* The rest of the code is private */

#ifdef ARIA2WEB_IMPL

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

void* a2w_run_http_server(void* context);
void* a2w_run_webview_loop(void* context);

void log_debug(const char* s) { puts(s); }

static int confirmed_port = -1;

int a2w_get_http_server_port_number()
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
			printf("# aria2web using port %d\n", port);
			return port;
		}
		port = 49152 + random() % (0xFFFF - 49152);
    }
}

typedef struct aria2web_tag {
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
	aria2web_initialized_callback initialized_callback{nullptr};
	void* initialized_callback_context{nullptr};
	aria2web_control_change_callback control_change_callback{nullptr};
	void* cc_callback_context{nullptr};
	aria2web_note_callback note_callback{nullptr};
	void* note_callback_context{nullptr};
	aria2web_change_program_callback change_program_callback{nullptr};
	void* change_program_callback_context{nullptr};

	void start(void* parentWindow = nullptr)
	{
		pthread_create(&http_server_thread, NULL, a2w_run_http_server, this);
		pthread_setname_np(http_server_thread, "aria2web_httpserver");
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = 1000;
		while (!http_server_started)
			usleep(1000);
		parent_window = parentWindow;
		a2w_run_webview_loop(this);
	}

	void stop()
	{
		webview_destroy(webview);
		pthread_cancel(http_server_thread);
	}

} aria2web;

aria2web* aria2web_create(const char* webLocalFilePath) {
	auto ret = new aria2web(); 
	ret->web_local_file_path = strdup(webLocalFilePath);
	return ret;
}

void aria2web_free(aria2web* instance) {
	delete instance;
}

void aria2web_start(aria2web* instance, void* parentWindow) { instance->start(parentWindow); }

void aria2web_stop(aria2web* instance) { instance->stop(); }

void* aria2web_get_native_widget(aria2web* instance) {
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
	auto a2w = (aria2web*) http_request_server_userdata(request);

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
	for (auto dir : *a2w->local_instrument_dirs) {
		if (strncmp(dir.c_str(), targetPath + 1, dir.length()) == 0) {
			filePath = std::string{targetPath + 1};
			break;
		}
	}
	if (filePath.length() == 0)
		filePath = a2w->web_local_file_path.length() > 0 ? a2w->web_local_file_path + "/" + targetPath : targetPath;

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

void* a2w_run_http_server(void* context) {
	auto a2w = (aria2web*) context;
	struct http_server_s* server = http_server_init(a2w_get_http_server_port_number(), handle_request);
	http_server_set_userdata(server, a2w);
	a2w->http_server_started = TRUE;
	http_server_listen(server);
	return nullptr;
}

void aria2web_show_window(aria2web* a2w)
{
	gtk_widget_show((GtkWidget*) webview_get_window((webview_t) a2w->webview));
}

void aria2web_hide_window(aria2web* a2w)
{
	gtk_widget_hide((GtkWidget*) webview_get_window((webview_t) a2w->webview));
}

typedef struct {
	aria2web *a2w;
	const char* sfzfile;
} load_sfz_ctx;

int do_load_sfz(void* context) {
	auto ctx = (load_sfz_ctx*) context;
	auto js_log = std::string{} + "console.log('aria2web.h: loading UI via SFZ by host request: " + ctx->sfzfile + "');";
	webview_eval((webview_t) ctx->a2w->webview, js_log.c_str());
	auto js = std::string{} + "loadInstrumentFromSfz('" + ctx->sfzfile + "');";
	webview_eval((webview_t) ctx->a2w->webview, js.c_str());
	free(ctx);
	return false;
}

void aria2web_load_sfz(aria2web* a2w, const char* sfzfile)
{
	auto ctx = new load_sfz_ctx{a2w, sfzfile};
	g_idle_add(do_load_sfz, ctx);
}

void aria2web_set_initialized_callback(aria2web* a2w, aria2web_initialized_callback callback, void* callbackContext)
{
	a2w->initialized_callback = callback;
	a2w->initialized_callback_context = callbackContext;
}

void aria2web_set_control_change_callback(aria2web* a2w, aria2web_control_change_callback callback, void* callbackContext)
{
	a2w->control_change_callback = callback;
	a2w->cc_callback_context = callbackContext;
}

void aria2web_set_note_callback(aria2web* a2w, aria2web_note_callback callback, void* callbackContext)
{
	a2w->note_callback = callback;
	a2w->note_callback_context = callbackContext;
}

void aria2web_set_change_program_callback(aria2web* a2w, aria2web_change_program_callback callback, void* callbackContext)
{
	a2w->change_program_callback = callback;
	a2w->change_program_callback_context = callbackContext;
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
	auto a2w = (aria2web*) arg;
	if (a2w && a2w->initialized_callback)
		a2w->initialized_callback(a2w->initialized_callback_context);
	else {
		log_debug("aria2web.h: 'initialized' callback is invoked");
	}
}

void webview_callback_control_change(const char *seq, const char *req, void *arg) {
	int cc = 0, val = 0;
	parse_js_two_array_items(req, &cc, &val);

	auto a2w = (aria2web*) arg;
	if (a2w && a2w->control_change_callback)
		a2w->control_change_callback(a2w->cc_callback_context, cc, val);
	else {
		log_debug("aria2web.h: control change callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void webview_callback_note(const char *seq, const char *req, void *arg) {
	int state = 0, key = 0;
	parse_js_two_array_items(req, &state, &key);

	auto a2w = (aria2web*) arg;
	if (a2w && a2w->note_callback)
		a2w->note_callback(a2w->note_callback_context, key, state ? 127 : 0);
	else {
		log_debug("aria2web.h: note callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
}

void webview_callback_change_program(const char *seq, const char *req, void *arg) {
	auto a2w = (aria2web*) arg;
	// req = ["filename"] so skip first two bytes and trim last two bytes.
	auto s = strdup(req);
	s[strlen(s) - 2] = '\0';

	if (a2w && a2w->control_change_callback)
		a2w->change_program_callback(a2w->cc_callback_context, s + 2);
	else {
		log_debug("aria2web.h: change program callback is invoked");
		log_debug(seq);
		log_debug(req);
		log_debug(arg != NULL ? "not null" : "null");
	}
	free(s);
}

void webview_callback_get_local_instruments(const char *seq, const char *req, void *arg) {
	auto a2w = (aria2web*) arg;

	std::string fileList{};
	std::string aria2web_config_file{getenv("HOME")};
	aria2web_config_file += "/.config/aria2web.config";
	struct stat cfgStat;
	if (stat(aria2web_config_file.c_str(), &cfgStat) == 0) {
		FILE *fp = fopen(aria2web_config_file.c_str(), "r");
		if (fp != nullptr) {
			char buf[1024];
			a2w->local_instrument_files->clear();
			a2w->local_instrument_dirs->clear();
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
				a2w->local_instrument_files->emplace_back(std::string{buf});
				fileList += buf;
				fileList += "\"";

				if (strrchr(buf, '/') != nullptr)
					*strrchr(buf, '/') = '\0';
				a2w->local_instrument_dirs->emplace_back(std::string{buf});
			}
		}
		else
			printf("#aria2web.h: failed to load config file %s...\n", aria2web_config_file.c_str());
	}
	std::string cfgJS{"Aria2Web.Config={BankXmlFiles: [" + fileList + "]}; onLocalBankFilesUpdated();"};

	webview_eval(a2w->webview, cfgJS.c_str());
}

void on_dispatch(webview_t w, void* context) {
	// FIXME: add gtk window close callback here.
}

// It used to be implemented to run on a dedicated thread, which still worked as a standalone UI
// but as a plugin which is loaded and run under DAW GUI it violates UI threading principle, so
// it was rewritten to run within the callers (a2w) thread.
void* a2w_run_webview_loop(void* context) {
	const char* urlfmt = "http://localhost:%d/index.html";
	int port = a2w_get_http_server_port_number();
	char* url = strdup(urlfmt);
	sprintf(url, urlfmt, port);

	auto a2w = (aria2web*) context;
	a2w->local_instrument_files.reset(new std::vector<std::string>());
	a2w->local_instrument_dirs.reset(new std::vector<std::string>());
	void* w = a2w->webview = webview_create(true, nullptr);
	webview_set_title(w, "Aria2Web embedded example");
	webview_set_size(w, 1200, 450, WEBVIEW_HINT_NONE);
	webview_bind(w, "Aria2WebInitializedCallback", webview_callback_initialized, context);
	webview_bind(w, "Aria2WebControlChangeCallback", webview_callback_control_change, context);
	webview_bind(w, "Aria2WebNoteCallback", webview_callback_note, context);
	webview_bind(w, "Aria2WebChangeProgramCallback", webview_callback_change_program, context);
	webview_bind(w, "Aria2WebGetLocalInstrumentsCallback", webview_callback_get_local_instruments, context);

	webview_dispatch(w, on_dispatch, context);
	webview_navigate(w, url);
	gtk_window_set_deletable((GtkWindow*) webview_get_window(w), FALSE);
	free(url);
	a2w->webview_ready = TRUE;
	webview_run(w);
	return nullptr;
}

#endif /* ARIA2WEB_IMPL */
