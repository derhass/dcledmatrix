#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/base.h>
#include <stdlib.h>

#include "dclmd_comm.h"

OBS_DECLARE_MODULE()

MODULE_EXPORT const char *obs_module_description(void)
{
        return "DCLEDMatrix status display";
}

typedef struct {
	DCLMDComminucation *comm;
	unsigned int flags;
	char curScene[256];
	char text[6];
} ctx_t;

#define FLAG_FRONTEND_CALLBACK	0x1
#define FLAG_RECORDING		0x2
#define FLAG_STREAMING		0x4
#define FLAGS_DEFAULT		0

static ctx_t *module_ctx=NULL;

static DCLMDComminucation*
ctx_get_comm(ctx_t *ctx)
{
	if (ctx->comm) {
		return ctx->comm;
	}
	ctx->comm = dclmdCommunicationClientCreate();
	if (ctx->comm) {
		dclmdClientBlank(ctx->comm, 0);
	}
	return ctx->comm;
}

static void
get_current_scene(ctx_t *ctx)
{
	obs_source_t *src = obs_frontend_get_current_scene();
	const char *name=(src)?obs_source_get_name(src):NULL;
	if (name) {
		strncpy(ctx->curScene, name, sizeof(ctx->curScene));
		ctx->curScene[sizeof(ctx->curScene)-1]=0;
	} else {
		ctx->curScene[0]=0;
	}
}

static void
frontend_event_callback(enum obs_frontend_event ev, void *v_ctx)
{
	ctx_t *ctx = (ctx_t*)v_ctx;
	DCLEDMatrixError err = DCLM_OK;
	unsigned int timeout_ms;

	if (!ctx) {
		return;
	}

	/* the events we care about */
	switch(ev) {
		case OBS_FRONTEND_EVENT_STREAMING_STARTING:
			ctx->text[0]='.';
			break;
		case OBS_FRONTEND_EVENT_STREAMING_STARTED:
			ctx->text[0]='*';
			break;
		case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
			ctx->text[0]=' ';
			break;
		case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
			ctx->text[0]=':';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STARTING:
			ctx->text[1]='.';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STARTED:
			ctx->text[1]='+';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
			ctx->text[1]=':';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
			ctx->text[1]=' ';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
			ctx->text[1]='P';
			break;
		case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
			ctx->text[1]='+';
			break;
		case OBS_FRONTEND_EVENT_SCENE_CHANGED:
			get_current_scene(ctx);
			break;
		case OBS_FRONTEND_EVENT_FINISHED_LOADING:
			blog(LOG_INFO,"dcledmatrix: event callback initialized");
			if ( (err=dclmdClientShowText(ctx->comm, "INIT", 0, 0, DCLMD_CMD_CLEAR_SCREEN, 5000)) != DCLM_OK) {
				blog(LOG_WARNING, "dcledmatrix: failed show init text: %d", (int)err);
			}
			return;
		default:
			/* event not relevant for us */
			return;
	}
	if (!ctx->comm) {
		return;
	}

	if ( (ctx->text[0] == ' ') && (ctx->text[1] == ' ') ) {
		timeout_ms = 5000;
	} else {
		timeout_ms = 0;
	}	

	ctx->text[3]=ctx->curScene[0];
	if ( (err=dclmdClientShowText(ctx->comm, ctx->text, 4, 0, DCLMD_CMD_CLEAR_SCREEN, timeout_ms)) != DCLM_OK) {
		blog(LOG_WARNING, "dcledmatrix: failed to show text: %d", (int)err);
	}
	/*
	blog(LOG_DEBUG,"dcledmatrix: TEXT:'%s'",ctx->text);*/
	/*
	memcpy(ctx->text+1,ctx->curScene,sizeof(ctx->text)-2);
	dclmdClientShowText(comm, ctx->text, 0, 0);
	*/
}

static void
ctx_init(ctx_t *ctx)
{
	ctx->comm=NULL;
	ctx->flags = FLAGS_DEFAULT;
	ctx->curScene[0]=0;
	ctx->text[0]=' ';
	ctx->text[1]=' ';
	ctx->text[2]=' ';
	ctx->text[3]='_';
	ctx->text[4]='0';
	ctx->text[sizeof(ctx->text)-1]=0;
}

static void
ctx_cleanup(ctx_t *ctx)
{
	if (!ctx) {
		return;
	}

	if (ctx->comm) {
		dclmdCommunicationDestroy(ctx->comm);
		ctx->comm=NULL;
	}

	if (ctx->flags & FLAG_FRONTEND_CALLBACK) {
		blog(LOG_DEBUG, "dcledmatrix removing frontend event callback");
		obs_frontend_remove_event_callback(frontend_event_callback, ctx);
		ctx->flags &= ~FLAG_FRONTEND_CALLBACK;
	}
}

static void
ctx_destroy(ctx_t *ctx)
{
	if (!ctx) {
		return;
	}

	ctx_cleanup(ctx);
	free(ctx);
}

static ctx_t *
ctx_create()
{
	ctx_t *ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		blog(LOG_WARNING,"dcledmatrix malloc failed");
		return ctx;
	}
	ctx_init(ctx);
	ctx_get_comm(ctx);
	if (!ctx->comm) {
		blog(LOG_WARNING,"dcledmatrix communication to daemon failed");
		ctx_destroy(ctx);
		return NULL;
	}

	blog(LOG_DEBUG, "dcledmatrix installing frontend event callback");
	obs_frontend_add_event_callback(frontend_event_callback, ctx);
	ctx->flags |= FLAG_FRONTEND_CALLBACK;
	get_current_scene(ctx);
	return ctx;
}

bool
obs_module_load(void)
{
	if (module_ctx) {
		ctx_destroy(module_ctx);
	}
	module_ctx = ctx_create();
	if (module_ctx) {
		blog(LOG_INFO,"dcledmatrix loaded");
		return true;
	}
	blog(LOG_WARNING,"dcledmatrix initialization failed");
	return false;
}

void
obs_module_unload(void)
{
	if (module_ctx) {
		if (module_ctx->comm) {
			DCLEDMatrixError err;
			if ( (err=dclmdClientShowText(module_ctx->comm, "FINI", 0, 0, DCLMD_CMD_CLEAR_SCREEN, 5000)) != DCLM_OK) {
				blog(LOG_WARNING, "dcledmatrix: failed show finish text: %d", (int)err);
			}
		}
		ctx_destroy(module_ctx);
	}
	module_ctx = NULL;
	blog(LOG_INFO,"dcledmatrix unloaded");
}
