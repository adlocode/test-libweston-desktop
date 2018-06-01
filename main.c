/* Copyright (C) 2018 adlo
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>
 */

#define _GNU_SOURCE
#include <wayland-server.h>
#include <compositor.h>
#include <compositor-drm.h>
#include <compositor-wayland.h>
#include <libweston-desktop.h>
#include <libinput.h>
#include <string.h>
#include <windowed-output-api.h>
#include <stdio.h>
#include <stdlib.h>

struct TestServer
{
  struct weston_compositor *compositor;
  struct wl_listener new_output;
  const struct weston_windowed_output_api *api;
  struct weston_layer background_layer;
  struct weston_surface *background;
  struct weston_view *background_view;
  struct weston_layer surfaces_layer;
};

struct TestServerSurface
{
  struct weston_desktop_surface *desktop_surface;
  struct weston_surface *surface;
  struct weston_view *view;
};

void surface_added (struct weston_desktop_surface *desktop_surface,
                    void                   *user_data)
{
  struct TestServer *server = user_data;

  struct TestServerSurface *self;

  self = calloc (1, sizeof (struct TestServerSurface));

  self->desktop_surface = desktop_surface;

  weston_desktop_surface_set_user_data (self->desktop_surface, self);

  self->surface = weston_desktop_surface_get_surface (self->desktop_surface);
  self->view = weston_desktop_surface_create_view (self->desktop_surface);

  weston_layer_entry_insert (&server->surfaces_layer.view_list, &self->view->layer_link);

  weston_view_set_position (self->view, 0, 0);

  weston_surface_damage (self->surface);
  weston_compositor_schedule_repaint (server->compositor);
}

void surface_removed (struct weston_desktop_surface *desktop_surface,
                      void                   *user_data)
{
  struct TestServer *server = user_data;

  struct TestServerSurface *self = weston_desktop_surface_get_user_data (desktop_surface);

  if (self == NULL)
    return;

  weston_desktop_surface_unlink_view (self->view);
  weston_view_destroy (self->view);
  weston_desktop_surface_set_user_data (desktop_surface, NULL);
  free (self);
}
static int vlog (const char *fmt,
                 va_list     ap)
{
  return vfprintf (stderr, fmt, ap);
}

static int vlog_continue (const char *fmt,
                          va_list     argp)
{
  return vfprintf (stderr, fmt, argp);
}

static void new_output_notify (struct wl_listener *listener,
                            void               *data)
{
  struct weston_output *output = data;
  struct TestServer *server = wl_container_of (listener, server, new_output);

  weston_output_set_scale (output, 1);
  weston_output_set_transform (output, WL_OUTPUT_TRANSFORM_NORMAL);
  server->api->output_set_size (output, 800, 600);
  weston_output_enable (output);

}
int main (int    argc,
          char **argv)
{
	struct wl_display *display;
	struct weston_compositor *ec = NULL;
	int ret = 0;
  const char *socket_name = NULL;
  struct weston_desktop_api desktop_api;
  struct weston_desktop *desktop;
  struct TestServer *server;

  server = malloc (sizeof(struct TestServer));

	display = wl_display_create ();
	server->compositor = weston_compositor_create (display, NULL);
  weston_compositor_set_xkb_rule_names (server->compositor, NULL);

	if (!server->compositor)
		return 0;

  weston_log_set_handler (vlog, vlog_continue);

	server->compositor->default_pointer_grab = NULL;
	server->compositor->vt_switching = true;

	server->compositor->repaint_msec = 16;
	server->compositor->idle_time = 300;

	struct weston_wayland_backend_config config = {{0, }};

	config.base.struct_version = WESTON_WAYLAND_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof (struct weston_wayland_backend_config);

	config.cursor_size = 32;
	config.display_name = 0;
	config.use_pixman = 0;
	config.sprawl = 0;
	config.fullscreen = 0;
	config.cursor_theme = NULL;


	ret = weston_compositor_load_backend (server->compositor, WESTON_BACKEND_WAYLAND, &config.base);

  server->api = weston_windowed_output_get_api (server->compositor);
  server->new_output.notify = new_output_notify;
  wl_signal_add (&server->compositor->output_created_signal, &server->new_output);

  server->api->output_create (server->compositor, "W1");

  desktop_api.surface_added = surface_added;
  desktop_api.surface_removed = surface_removed;



  weston_pending_output_coldplug (server->compositor);

  weston_layer_init (&server->background_layer, server->compositor);
  weston_layer_set_position (&server->background_layer, WESTON_LAYER_POSITION_BACKGROUND);
  server->background = weston_surface_create (server->compositor);
  weston_surface_set_size (server->background, 8096, 8096);
  weston_surface_set_color (server->background, 0, 0.25, 0.5, 1);
  server->background_view = weston_view_create (server->background);
  weston_layer_entry_insert (&server->background_layer.view_list, &server->background_view->layer_link);

  desktop = weston_desktop_create (server->compositor, &desktop_api, server);
  weston_layer_init (&server->surfaces_layer, server->compositor);
  weston_layer_set_position (&server->surfaces_layer, WESTON_LAYER_POSITION_NORMAL);


  socket_name = wl_display_add_socket_auto (display);
  if (socket_name)
  {
    weston_log ("Compositor running on %s", socket_name);
    setenv ("WAYLAND_DISPLAY", socket_name, 1);
    unsetenv ("DISPLAY");
  }


  weston_compositor_wake (server->compositor);
  wl_display_run (display);



	return 0;
}