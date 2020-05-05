#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
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
	
	float buf[2];
} aria2weblv2ui;

void a2wlv2_cc_callback(void* context, int cc, int value)
{
	auto a = (aria2weblv2ui*) context;

	puts("a2wlv2_cc_callback invoked");

	// TODO: generate Atom MIDI message here.
	a->buf[0] = (float) value;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, 1, LV2_UI__floatProtocol, &a->buf);
}

void a2wlv2_note_callback(void* context, int key, int velocity)
{
	auto a = (aria2weblv2ui*) context;

	puts("a2wlv2_note_callback invoked");
	
	// TODO: generate Atom MIDI message here.
	a->buf[0] = (float) key;
	a->buf[1] = (float) velocity;

	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, 2, LV2_UI__floatProtocol, &a->buf);
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
