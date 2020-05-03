#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <pthread.h>

#include <webview.h>
#define HTTPSERVER_IMPL
#include <httpserver.h>

void log_debug(const char* s) { puts(s); }

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
	p[dst] = NULL;
}

void uri_strip_query_string(char *s) {
	for (int i = 0; s[i]; i++)
		if (s[i] == '?') {
			s[i] = NULL;
			return;
		}
}

const char* mime_types[] = {"text/html", "application/xml", "image/png" };

const char* get_mime_type_from_filename(char* filename) {
	std::string s{filename};
	if (s.size() > 4) {
		printf("filename: %s, size %d, compareXML: %d\n", filename, s.size(), s.compare(s.size() - 4, 4, ".xml"));
		printf("filename: %s, size %d, comparePNG: %d\n", filename, s.size(), s.compare(s.size() - 4, 4, ".png"));
	} else printf("FILENAME: %s\n", filename);
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".xml") == 0)
		return mime_types[1];
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".png") == 0)
		return mime_types[2];
	return mime_types[0];
}

void handle_request(struct http_request_s* request) {
	struct http_response_s* response = http_response_init();

	struct http_string_s target = http_request_target(request);
	char* targetPath = (char*) calloc(target.len + 2, 1);
	// FIXME: make sure that the requested resource is under pwd.
	targetPath[0] = '.';
	memcpy(targetPath + 1, target.buf, target.len);
	targetPath[target.len + 1] = NULL;
	log_debug(targetPath);
	uri_unescape_in_place(targetPath);
	uri_strip_query_string(targetPath);
	
	const char* mimeType = get_mime_type_from_filename(targetPath);

	int fd = open(targetPath, O_RDONLY);
	free(targetPath);
	if (fd == -1) {
		puts("... was NOT found.");
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

void* runHttpServer(void*) {
	struct http_server_s* server = http_server_init(37564, handle_request);
	http_server_listen(server);
	return NULL;
}

void webview_callback_control_change(const char *seq, const char *req, void *arg) {
	log_debug("control change callback is invoked");
	log_debug(seq);
	log_debug(req);
	log_debug(arg != NULL ? "not null" : "null");
}

void webview_callback_note(const char *seq, const char *req, void *arg) {
	log_debug("note callback is invoked");
	log_debug(seq);
	log_debug(req);
	log_debug(arg != NULL ? "not null" : "null");
}

pthread_t http_server_thread;

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
	pthread_create(&http_server_thread, NULL, runHttpServer, NULL);

	auto w = webview_create(true, nullptr);
	webview_set_title(w, "Aria2Web embedded example");
	webview_set_size(w, 1200, 450, WEBVIEW_HINT_NONE);
	webview_navigate(w, "http://localhost:37564/index.html");
	webview_bind(w, "ControlChangeCallback", webview_callback_control_change, NULL);
	webview_bind(w, "NoteCallback", webview_callback_note, NULL);
	webview_run(w);
	
	return 0;
}

