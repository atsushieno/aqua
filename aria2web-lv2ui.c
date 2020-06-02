#include <vector>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "lv2_external_ui.h"
#include "aria2web.h"
#include "process.hpp"

extern "C" {

// rewrite this part to whichever port your plugin is listening.
#ifndef ARIA2WEB_LV2_CONTROL_PORT
#define ARIA2WEB_LV2_CONTROL_PORT 0 //  0 is sfizz control-in port.
#endif

typedef struct aria2weblv2ui_tag {
	// This is tricky, but do NOT move this member from TOP of this struct, as
	// this plugin implementation makes use of the fact that the LV2_External_UI_Widget* also
	// points to the address of this containing `aria2weblv2ui` struct.
	// It is due to the fact that LV2_External_UI_Widget is not designed well and lacks
	// "context" parameter to be passed to its callbacks and therefore we have no idea
	// "which" UI instance to manipulate.
	LV2_External_UI_Widget extui;

	#if IN_PROCESS_WEBVIEW
	aria2web* a2w;
#else
	pthread_t ui_launcher_thread;
	std::unique_ptr<TinyProcessLib::Process> a2w_process;
#endif
	const char *plugin_uri;
	const char *bundle_path;
	LV2UI_Write_Function write_function;
	LV2UI_Controller controller;
	const LV2_Feature *const *features;
	int urid_atom, urid_frame_time, urid_midi_event, urid_atom_event_transfer;
	bool is_visible_now{false};

	char atom_buffer[50];
} aria2weblv2ui;

char* fill_atom_message_base(aria2weblv2ui* a, LV2_Atom_Sequence* seq)
{
	auto events = (LV2_Atom_Event*) ((char*) seq + sizeof (LV2_Atom_Sequence));
	seq->body.unit = a->urid_frame_time;
	auto ev = &events[0];
	auto msg = ((char*) ev + sizeof (LV2_Atom_Event));
	ev->time.frames = 0;
	ev->body.type = a->urid_midi_event;
	ev->body.size = 3;
	seq->atom.size = sizeof(LV2_Atom_Event) + 3;
	seq->atom.type = a->urid_atom_event_transfer;
	return msg;
}

// Unlike the name implies, it is actually not that general, but more specific to sfizz...
// See https://github.com/sfztools/sfizz/blob/a1fdfa2e/lv2/sfizz.c#L491 for MIDI message processing details.
void a2wlv2_cc_callback(void* context, int cc, int value)
{
	auto a = (aria2weblv2ui*) context;

	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = 0xB0; // channel does not matter on sfizz
	msg[1] = (char) cc;
	msg[2] = (char) value;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), a->urid_atom_event_transfer, a->atom_buffer);
}

// Unlike the name implies, it is actually not that general, but more specific to sfizz...
// See https://github.com/sfztools/sfizz/blob/a1fdfa2e/lv2/sfizz.c#L491 for MIDI message processing details.
void a2wlv2_note_callback(void* context, int key, int velocity)
{
	auto a = (aria2weblv2ui*) context;
	
	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = velocity == 0 ? 0x80 : 0x90; // channel does not matter on sfizz
	msg[1] = (char) key;
	msg[2] = (char) velocity;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), a->urid_atom_event_transfer, a->atom_buffer);
}

void extui_show_callback(LV2_External_UI_Widget* widget) {
	auto a2w = (aria2weblv2ui*) (void*) widget;
#if IN_PROCESS_WEBVIEW
	// FIXME: implement
	printf ("!!!!! EXTUI_SHOW_CALLBACK %s\n", a2w->plugin_uri);
#else
	a2w->a2w_process->write("show\n");
#endif
}

void extui_hide_callback(LV2_External_UI_Widget* widget) {
	auto a2w = (aria2weblv2ui*) (void*) widget;
#if IN_PROCESS_WEBVIEW
	// FIXME: implement
	printf ("!!!!! EXTUI_HIDE_CALLBACK %s\n", a2w->plugin_uri);
#else
	a2w->a2w_process->write("hide\n");
#endif
}

void extui_run_callback(LV2_External_UI_Widget* widget) {
	// nothing to do here...
}

#if !IN_PROCESS_WEBVIEW
void* runloop_aria2web_host(void* context)
{
	auto aui = (aria2weblv2ui*) context;
	std::string cmd = std::string{} + aui->bundle_path + "/aria2web-host --plugin";
	aui->a2w_process.reset(new TinyProcessLib::Process(cmd, "", [&aui](const char* bytes, size_t n) {
		if (n <= 0)
			return;
		int v1, v2;
		switch (bytes[0]) {
			case 'N':
				sscanf(bytes + 1, "#%x,#%x", &v1, &v2);
				printf("NOTE EVENT RECEIVED: %d %d\n", v1, v2);
				a2wlv2_note_callback(aui, v1, v2);
				break;
			case 'C':
				sscanf(bytes + 1, "#%x,#%x", &v1, &v2);
				printf("CC EVENT RECEIVED: %d %d\n", v1, v2);
				a2wlv2_cc_callback(aui, v1, v2);
				break;
			case'Q':
				// terminate
				return aui->a2w_process->kill();
			default:
				printf("Unrecognized command sent by aria2web-host: %s\n", bytes);
		}
	}, nullptr, true));

	return nullptr;
}

#endif

LV2UI_Handle aria2web_lv2ui_instantiate(
	const LV2UI_Descriptor *descriptor,
	const char *plugin_uri,
	const char *bundle_path,
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller,
	LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	auto ret = new aria2weblv2ui();
	ret->plugin_uri = plugin_uri;
	// FIXME: do not alter bundle path. Adjust it in aria2web.h. (it does not even free memory now)
	ret->bundle_path = strdup((std::string{bundle_path} + "/resources").c_str());
	ret->write_function = write_function;
	ret->controller = controller;
	ret->features = features;

	auto extui = &ret->extui;
	extui->show = extui_show_callback;
	extui->hide = extui_hide_callback;
	extui->run = extui_run_callback;

	for (int i = 0; features[i]; i++) {
		auto f = features[i];
		if (strcmp(f->URI, LV2_URID__map) == 0) {
			auto urid = (LV2_URID_Map*) f->data;
			ret->urid_atom = urid->map(urid->handle, LV2_ATOM_URI);
			ret->urid_frame_time = urid->map(urid->handle, LV2_ATOM__frameTime);
			ret->urid_midi_event = urid->map(urid->handle, LV2_MIDI__MidiEvent);
			ret->urid_atom_event_transfer = urid->map(urid->handle, LV2_ATOM__eventTransfer);
			break;
		}
	}

#if IN_PROCESS_WEBVIEW
	ret->a2w = aria2web_create(bundle_path);
	aria2web_set_control_change_callback(ret->a2w, a2wlv2_cc_callback, ret);
	aria2web_set_note_callback(ret->a2w, a2wlv2_note_callback, ret);
	ret->a2w->web_local_file_path = ret->bundle_path;

	aria2web_start(ret->a2w, nullptr);
#else
	pthread_t thread;
	pthread_create(&thread, nullptr, runloop_aria2web_host, ret);
	pthread_setname_np(thread, "aria2web_lv2ui_host_launcher");
	ret->ui_launcher_thread = thread;
	while (ret->a2w_process.get() == nullptr)
		usleep(1000);
#endif

	*widget = &ret->extui;
	return ret;
}

void aria2web_lv2ui_cleanup(LV2UI_Handle ui)
{
	auto a2wlv2 = (aria2weblv2ui*) ui;
#if IN_PROCESS_WEBVIEW
	aria2web_stop(a2wlv2->a2w);
#else
	a2wlv2->a2w_process->kill();
#endif
	free(a2wlv2);
}

void aria2web_lv2ui_port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer)
{
	if (port_index == ARIA2WEB_LV2_CONTROL_PORT) {
		// FIXME: reflect the value changes back to UI.
		printf("port event received. buffer size: %d bytes\n", buffer_size);
	}
}

LV2UI_Show_Interface aria2weblv2ui_show_interface;

int aria2web_lv2ui_showinterface_show(LV2UI_Handle ui)
{
	// FIXME: implement (but zrythm never calls it...?)
	auto a2w = (aria2weblv2ui*) (void*) ui;
	printf ("!!!!! LV2UI_Show_Interface: show %s\n", a2w->plugin_uri);
}

int aria2web_lv2ui_showinterface_hide(LV2UI_Handle ui)
{
	// FIXME: implement (but zrythm never calls it...?)
	auto a2w = (aria2weblv2ui*) (void*) ui;
	printf ("!!!!! LV2UI_Show_Interface: hide %s\n", a2w->plugin_uri);
}

void* aria2web_lv2ui_extension_data(const char *uri)
{
	printf ("!!!!! aria2web_lv2ui_extension_data: %s\n", uri);
	// zrythm never calls it...?
	if (strcmp(uri, LV2_UI__showInterface) == 0) {
		aria2weblv2ui_show_interface.show = aria2web_lv2ui_showinterface_show;
		aria2weblv2ui_show_interface.hide = aria2web_lv2ui_showinterface_hide;
		return &aria2weblv2ui_show_interface;
	}
	return nullptr;
}

LV2UI_Descriptor uidesc;

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
	uidesc.URI = "https://github.com/atsushieno/aria2web#ui";
	uidesc.instantiate = aria2web_lv2ui_instantiate;
	uidesc.cleanup = aria2web_lv2ui_cleanup;
	uidesc.port_event = aria2web_lv2ui_port_event;
	uidesc.extension_data = aria2web_lv2ui_extension_data;

	return &uidesc;
}

} // extern "C"
