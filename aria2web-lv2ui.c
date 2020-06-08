#include <vector>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "lv2_external_ui.h"
#include "aria2web.h"
#include "process.hpp"


#define PATH_MAX 4096
#define SFIZZ__sfzFile "https://github.com/atsushieno/aria2web:sfzfile"

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

	pthread_t ui_launcher_thread;
	std::unique_ptr<TinyProcessLib::Process> a2w_process;

	const char *plugin_uri;
	char *bundle_path;
	LV2_URID_Map *urid_map;

	LV2UI_Write_Function write_function;
	LV2UI_Controller controller;
	const LV2_Feature *const *features;
	int urid_atom, urid_frame_time, urid_midi_event, urid_atom_object, urid_patch_set, urid_patch_property,
		urid_patch_value, urid_atom_urid, urid_atom_path, urid_sfzfile, urid_voice_message, urid_atom_event_transfer;
	bool is_visible_now{false};

	char atom_buffer[PATH_MAX + 256];
} aria2weblv2ui;

char* fill_atom_message_base(aria2weblv2ui* a, LV2_Atom_Sequence* seq)
{
	seq->atom.type = a->urid_midi_event;
	seq->body.unit = a->urid_frame_time;
	seq->body.pad = 0; // only for cleanness

	auto ev = (LV2_Atom_Event*) (seq + 1);
	ev->time.frames = 0; // dummy
	auto msg = (char*) &seq->body; // really?

	seq->atom.size = sizeof(LV2_Atom) + 3; // really? shouldn't this be werapped in LV2_Atom_Event?

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

void a2wlv2_select_sfz_callback(void* context, const char* sfzfile)
{
	auto a = (aria2weblv2ui*) context;

#if 0
	// It is based on the code in sfizz that generates port notification,
	// but this does NOT generate the right Atom buffer.
	// This does not result in the right atom type at top level,
	// and I have no further idea on how so.
	// The entire Atom API reference is far from complete and not reliable.
	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;

	memset(a->atom_buffer, 0, PATH_MAX + 256);
	LV2_Atom_Forge forge;
	lv2_atom_forge_init(&forge, a->urid_map);
	lv2_atom_forge_set_buffer(&forge, a->atom_buffer, PATH_MAX + 256);
	LV2_Atom_Forge_Frame seq_frame;
	lv2_atom_forge_sequence_head(&forge, &seq_frame, 0);
	lv2_atom_forge_frame_time(&forge, 0);
	LV2_Atom_Forge_Frame obj_frame;
	lv2_atom_forge_object(&forge, &obj_frame, 0, a->urid_patch_set);
	lv2_atom_forge_key(&forge, a->urid_patch_property);
	lv2_atom_forge_urid(&forge, a->urid_sfzfile);
	lv2_atom_forge_key(&forge, a->urid_patch_value);
	lv2_atom_forge_path(&forge, sfzfile, (uint32_t)strlen(sfzfile));
	lv2_atom_forge_pop(&forge, &obj_frame);
	lv2_atom_forge_pop(&forge, &seq_frame);
#else
	// Since Atom forge API is not documented appropriately, I avoid using it here.
	//
	// The event should contain an atom:Object whose body object type
	// is a patch:Set whose patch:property is sfzfile
	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	memset(seq, 0, PATH_MAX + 256);
	seq->atom.type = a->urid_atom_object;
	auto body = (LV2_Atom_Object_Body*) &seq->body;
	body->otype = a->urid_patch_set;
	auto propBody = (LV2_Atom_Property_Body*) lv2_atom_object_begin(body);
	propBody->key = a->urid_patch_property;
	propBody->value.type = a->urid_atom_urid;
	propBody->value.size = sizeof(LV2_URID);
	((LV2_Atom_URID *) &propBody->value)->body = a->urid_sfzfile;

	propBody = lv2_atom_object_next(propBody);
	propBody->key = a->urid_patch_value;
	propBody->value.type = a->urid_atom_path;
	propBody->value.size = strlen(sfzfile) + 1;
	auto filenameDst = ((char*) &propBody->value) + sizeof(LV2_Atom);
	strncpy(filenameDst, sfzfile, strlen(sfzfile));
	filenameDst[strlen(sfzfile)] = '\0';

	seq->atom.size = sizeof(LV2_Atom_Event) + sizeof(LV2_Atom_Object) + sizeof(LV2_Atom_Property) * 2 + strlen(sfzfile);
#endif

	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), a->urid_atom_event_transfer, a->atom_buffer);
}

void extui_show_callback(LV2_External_UI_Widget* widget) {
	auto a2w = (aria2weblv2ui*) (void*) widget;

	a2w->a2w_process->write("show\n");
}

void extui_hide_callback(LV2_External_UI_Widget* widget) {
	auto a2w = (aria2weblv2ui*) (void*) widget;

	a2w->a2w_process->write("hide\n");
}

void extui_run_callback(LV2_External_UI_Widget* widget) {
	// nothing to do here...
}

void* runloop_aria2web_host(void* context)
{
	auto aui = (aria2weblv2ui*) context;
	std::string cmd = std::string{} + aui->bundle_path + "/aria2web-host --plugin";
	aui->a2w_process.reset(new TinyProcessLib::Process(cmd, "", [&aui](const char* bytes, size_t n) {
		if (n <= 0)
			return;
		int v1, v2;
		const char *sfz;
		switch (bytes[0]) {
			case 'N':
				sscanf(bytes + 1, "#%x,#%x", &v1, &v2);
				a2wlv2_note_callback(aui, v1, v2);
				break;
			case 'C':
				sscanf(bytes + 1, "#%x,#%x", &v1, &v2);
				a2wlv2_cc_callback(aui, v1, v2);
				break;
			case 'P':
				sfz = bytes + 2;
				*((char*)strchr(sfz, '\n')) = '\0';
				a2wlv2_select_sfz_callback(aui, sfz);
				break;
			default:
				printf("Unrecognized command sent by aria2web-host: %s\n", bytes);
		}
	}, nullptr, true));

	return nullptr;
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
	auto ret = new aria2weblv2ui();
	ret->plugin_uri = plugin_uri;
	ret->bundle_path = strdup(bundle_path);
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
			ret->urid_map = urid;
			ret->urid_atom = urid->map(urid->handle, LV2_ATOM_URI);
			ret->urid_frame_time = urid->map(urid->handle, LV2_ATOM__frameTime);
			ret->urid_midi_event = urid->map(urid->handle, LV2_MIDI__MidiEvent);
			ret->urid_atom_object = urid->map(urid->handle, LV2_ATOM__Object);
			ret->urid_patch_set = urid->map(urid->handle, LV2_PATCH__Set);
			ret->urid_patch_property = urid->map(urid->handle, LV2_PATCH__property);
			ret->urid_patch_value = urid->map(urid->handle, LV2_PATCH__value);
			ret->urid_atom_urid = urid->map(urid->handle, LV2_ATOM__URID);
			ret->urid_atom_path = urid->map(urid->handle, LV2_ATOM__Path);
			ret->urid_sfzfile = urid->map(urid->handle, SFIZZ__sfzFile);
			ret->urid_voice_message = urid->map(urid->handle, LV2_MIDI__VoiceMessage);
			ret->urid_atom_event_transfer = urid->map(urid->handle, LV2_ATOM__eventTransfer);
			break;
		}
	}

	pthread_t thread;
	pthread_create(&thread, nullptr, runloop_aria2web_host, ret);
	pthread_setname_np(thread, "aria2web_lv2ui_host_launcher");
	ret->ui_launcher_thread = thread;
	while (ret->a2w_process.get() == nullptr)
		usleep(1000);

	*widget = &ret->extui;
	return ret;
}

void aria2web_lv2ui_cleanup(LV2UI_Handle ui)
{
	puts("aria2web_lv2ui_cleanup");
	auto a2w = (aria2weblv2ui*) ui;
	a2w->a2w_process->kill();
	a2w->a2w_process->write("quit\n");
	free(a2w->bundle_path);
	free(a2w);
}

void aria2web_lv2ui_port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer)
{
	if (port_index == ARIA2WEB_LV2_CONTROL_PORT) {
		// FIXME: reflect the value changes back to UI.
		printf("port event received. buffer size: %d bytes\n", buffer_size);
	}
}

LV2UI_Descriptor uidesc;

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
	uidesc.URI = "https://github.com/atsushieno/aria2web#ui";
	uidesc.instantiate = aria2web_lv2ui_instantiate;
	uidesc.cleanup = aria2web_lv2ui_cleanup;
	uidesc.port_event = aria2web_lv2ui_port_event;
	uidesc.extension_data = nullptr;

	return &uidesc;
}

} // extern "C"
