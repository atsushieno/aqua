#include <vector>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "aria2web.h"

extern "C" {

// rewrite this part to whichever port your plugin is listening.
#ifndef ARIA2WEB_LV2_CONTROL_PORT
#define ARIA2WEB_LV2_CONTROL_PORT 0
#endif

typedef struct aria2weblv2ui_tag {
	aria2web* a2w;
	const char *plugin_uri;
	const char *bundle_path;
	LV2UI_Write_Function write_function;
	LV2UI_Controller controller;
	LV2UI_Widget *widget;
	const LV2_Feature *const *features;
	int urid_atom, urid_frame_time, urid_midi_event;
	
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
	seq->atom.type = a->urid_atom;
	return msg;
}

void a2wlv2_cc_callback(void* context, int cc, int value)
{
	auto a = (aria2weblv2ui*) context;

	puts("a2wlv2_cc_callback invoked");

	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = 0xB0;
	msg[1] = (char) cc;
	msg[2] = (char) value;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), LV2_UI__floatProtocol, a->atom_buffer);
}

void a2wlv2_note_callback(void* context, int key, int velocity)
{
	auto a = (aria2weblv2ui*) context;

	puts("a2wlv2_note_callback invoked");
	

	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = velocity == 0 ? 0x80 : 0x90;
	msg[1] = (char) key;
	msg[2] = (char) velocity;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), LV2_UI__floatProtocol, a->atom_buffer);
}

LV2UI_Handle aria2web_lv2ui_instantiate(
	const LV2UI_Descriptor *descriptor,
	const char *plugin_uri,
	const char *bundle_path,
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller,
	LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	aria2weblv2ui* ret = calloc(sizeof(aria2weblv2ui), 1);
	ret->a2w = aria2web_create();
	aria2web_set_control_change_callback(ret->a2w, a2wlv2_cc_callback, ret);
	aria2web_set_note_callback(ret->a2w, a2wlv2_note_callback, ret);
	ret->plugin_uri = plugin_uri;
	ret->bundle_path = bundle_path;
	ret->write_function = write_function;
	ret->controller = controller;
	ret->widget = widget;
	ret->features = features;

	for (int i = 0; features[i]; i++) {
		auto f = features[i];
		if (strcmp(f->URI, LV2_ATOM_URI) == 0) {
			auto urid = (LV2_URID_Map*) f->data;
			ret->urid_atom = urid->map(urid->handle, LV2_ATOM_URI);
			ret->urid_frame_time = urid->map(urid->handle, LV2_ATOM__frameTime);
			ret->urid_midi_event = urid->map(urid->handle, LV2_MIDI__MidiEvent);
			break;
		}
	}
}

void aria2web_lv2ui_cleanup(LV2UI_Handle ui)
{
	auto a2wlv2 = (aria2weblv2ui*) ui;
	aria2web_stop(a2wlv2->a2w);
	free(a2wlv2);
}


LV2UI_Descriptor uidesc;

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
	uidesc.URI = "https://github.com/atsushieno/aria2web/ns/1.0";
	uidesc.instantiate = aria2web_lv2ui_instantiate;
	uidesc.cleanup = aria2web_lv2ui_cleanup;
	uidesc.port_event = nullptr;
	uidesc.extension_data = nullptr;

	return &uidesc;
}

} // extern "C"
