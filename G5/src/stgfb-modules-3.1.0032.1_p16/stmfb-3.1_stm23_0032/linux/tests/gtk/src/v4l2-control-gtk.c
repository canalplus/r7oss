#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h> /* exit () */

#include <linux/videodev.h>

#include <gtk/gtk.h>

#include <../v4l2_helper.h>



static GtkWidget *
v4l2_inputs_enumerate (int fd)
{
  GtkWidget         *combo = gtk_combo_box_new_text ();
  struct v4l2_input  input;
  gboolean           ok = TRUE;

  memset (&input, 0, sizeof (input));
  while (ok)
    {
      if (fd != -1)
        ok = (ioctl (fd, VIDIOC_ENUMINPUT, &input) == 0) ? TRUE : FALSE;
      else
        {
          snprintf ((char *) input.name, sizeof (input.name),
                    "Dummy Input %u", input.index);
          ok = input.index <= 3;
        }

      if (ok)
        {
          gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
                                     (char *) input.name);
  
          ltv_message ("Found input '%s' w/ idx/std/type/audioset/tuner/status: %d/%.16llx/%d/%d/%d/%d",
                       input.name, input.index, input.std, input.type,
                       input.audioset, input.tuner, input.status);
  
          ++input.index;
        }
    }

  return GTK_WIDGET (combo);
}


static GtkWidget *
v4l2_outputs_enumerate (int fd)
{
  GtkWidget          *combo = gtk_combo_box_new_text ();
  struct v4l2_output  output;
  gboolean            ok = TRUE;

  memset (&output, 0, sizeof (output));
  while (ok)
    {
      if (fd != -1)
        ok = (ioctl (fd, VIDIOC_ENUMOUTPUT, &output) == 0) ? TRUE : FALSE;
      else
        {
          snprintf ((char *) output.name, sizeof (output.name),
                    "Dummy Output %u", output.index);
          ok = output.index < 6;
        }

      if (ok)
        {
          gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
                                     (char *) output.name);
  
          ltv_message ("Found output '%s' w/ idx/std/type/audioset/modulator: %d/%.16llx/%d/%d/%d",
                       output.name, output.index, output.std, output.type,
                       output.audioset, output.modulator);
          ++output.index;
        }
    }

  return GTK_WIDGET (combo);
}


/* per notebook page data: (output/input) */
enum nb_type {
  NOTEBOOK_OUTPUT,
  NOTEBOOK_INPUT,
  NOTEBOOK_COUNT
};


static gint _g_videofd;

static void
ext_menu_changed (GtkComboBox *combo,
                  gpointer     user_data)
{
  struct v4l2_ext_control  ctrl;
  struct v4l2_ext_controls controls;
  guint                    id = GPOINTER_TO_UINT (user_data);
  gint                     index = gtk_combo_box_get_active (combo);

  controls.ctrl_class = V4L2_CTRL_ID2CLASS (id);
  controls.count = 1;
  controls.controls = &ctrl;
  ctrl.id = id;
  ctrl.value = index;
  LTV_CHECK (ioctl (_g_videofd, VIDIOC_S_EXT_CTRLS, &controls));
}

static GtkComboBox *
enumerate_menu (int                    fd,
                struct v4l2_queryctrl *queryctrl)
{
  struct v4l2_querymenu     querymenu;
  struct v4l2_ext_control   ctrl;
  struct v4l2_ext_controls  controls;
  GtkWidget                *combo = gtk_combo_box_new_text ();

  ltv_message ("  Menu items (%x):", queryctrl->id);
  memset (&querymenu, 0, sizeof (querymenu));

  querymenu.id = queryctrl->id;
  for (querymenu.index = queryctrl->minimum;
       querymenu.index <= queryctrl->maximum;
       ++querymenu.index)
    {
      if (LTV_CHECK (ioctl (fd, VIDIOC_QUERYMENU, &querymenu)) == 0)
        {
          gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
                                     (gchar *) querymenu.name);
          ltv_message ("    %s", querymenu.name);
        }
    }

  controls.ctrl_class = V4L2_CTRL_ID2CLASS (querymenu.id);
  controls.count = 1;
  controls.controls = &ctrl;
  ctrl.id = querymenu.id;
  if (!LTV_CHECK (ioctl (_g_videofd, VIDIOC_G_EXT_CTRLS, &controls)))
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), ctrl.value);

  g_signal_connect (G_OBJECT (combo),
                    "changed",
                    G_CALLBACK (ext_menu_changed),
                    GUINT_TO_POINTER (queryctrl->id));

  return GTK_COMBO_BOX (combo);
}

static void
boolean_toggled (GtkToggleButton *toggle,
                 gpointer         user_data)
{
  struct v4l2_ext_control  ctrl;
  struct v4l2_ext_controls controls;
  guint                    id = GPOINTER_TO_UINT (user_data);
  gint                     active = gtk_toggle_button_get_active (toggle);

  gtk_toggle_button_set_inconsistent (toggle, FALSE);

  controls.ctrl_class = V4L2_CTRL_ID2CLASS (id);
  controls.count = 1;
  controls.controls = &ctrl;
  ctrl.id = id;
  ctrl.value = active;
  LTV_CHECK (ioctl (_g_videofd, VIDIOC_S_EXT_CTRLS, &controls));
}

static void
range_changed (GtkRange *range,
               gpointer  user_data)
{
  struct v4l2_ext_control  ctrl;
  struct v4l2_ext_controls controls;
  guint                    id = GPOINTER_TO_UINT (user_data);
  gdouble                  value = gtk_range_get_value (range);

  controls.ctrl_class = V4L2_CTRL_ID2CLASS (id);
  controls.count = 1;
  controls.controls = &ctrl;
  ctrl.id = id;
  ctrl.value = value;
  LTV_CHECK (ioctl (_g_videofd, VIDIOC_S_EXT_CTRLS, &controls));
}

static void
enum_output_controls (int        fd,
                      GtkWidget *scrolled)
{
  struct v4l2_queryctrl queryctrl;
  static const char *ctrl_type[] = { "???", "int", "bool", "menu", "button",
                                     "int64", "ctrl_class" };
  GtkWidget *vbox = gtk_vbox_new (TRUE, 2);
  GtkWidget *hbox = NULL;
  GtkWidget *label, *widget;

  queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
  while (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
    {
      ltv_message ("some %s%s control (%x) %s (minimum/max/def: %d/%d/%d)",
                   (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? "disabled " : "",
                   ctrl_type[queryctrl.type], queryctrl.id, queryctrl.name,
                   queryctrl.minimum, queryctrl.maximum,
                   queryctrl.default_value);

      hbox = gtk_hbox_new (TRUE, 4);

      if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED
          || (queryctrl.flags & V4L2_CTRL_FLAG_READ_ONLY
              && queryctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS))
        gtk_widget_set_sensitive (GTK_WIDGET (hbox), FALSE);

      widget = label = NULL;
      if (queryctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS)
        {
          gchar *str = g_markup_printf_escaped ("--- <b>%s</b> ---",
                                                (gchar *) queryctrl.name);
          label = gtk_label_new (NULL);
          gtk_label_set_markup (GTK_LABEL (label), str);
          g_free (str);
        }
      else
        {
          if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
            {
              label = gtk_label_new ((gchar *) queryctrl.name);
              if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED))
                widget = GTK_WIDGET (enumerate_menu (fd, &queryctrl));
              else
                widget = gtk_combo_box_new_text ();
            }
          else
            {
              struct v4l2_ext_control  ctrl;
              struct v4l2_ext_controls controls;
              int    ret;

              controls.ctrl_class = V4L2_CTRL_ID2CLASS (queryctrl.id);
              controls.count = 1;
              controls.controls = &ctrl;
              ctrl.id = queryctrl.id;
              ctrl.value = 0;
              /* don't LTV_CHECK() here, as it might be disabled */
              ret = ioctl (_g_videofd, VIDIOC_G_EXT_CTRLS, &controls);

              if (queryctrl.type == V4L2_CTRL_TYPE_BOOLEAN)
                {
                  label = gtk_check_button_new_with_label ((gchar *) queryctrl.name);

                  if (!ret)
                    gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (label),
                                                 ctrl.value ? TRUE : FALSE);
                  else
                    gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (label),
                                                        TRUE);

                  g_signal_connect (G_OBJECT (label),
                                    "toggled",
                                    G_CALLBACK (boolean_toggled),
                                    GUINT_TO_POINTER (queryctrl.id));
              
                }
              else if (queryctrl.type == V4L2_CTRL_TYPE_INTEGER)
                {
                  label = gtk_label_new ((gchar *) queryctrl.name);

                  if (queryctrl.flags & V4L2_CTRL_FLAG_SLIDER)
                    {
                      widget = gtk_hscale_new_with_range ((gdouble) queryctrl.minimum,
                                                          (gdouble) queryctrl.maximum,
                                                          (gdouble) queryctrl.step);
                      /* display integers only */
                      gtk_scale_set_digits (GTK_SCALE (widget), 0);
                      gtk_scale_set_draw_value (GTK_SCALE (widget), TRUE);
                      gtk_scale_set_value_pos (GTK_SCALE (widget),
                                               GTK_POS_BOTTOM);
                      gtk_range_set_update_policy (GTK_RANGE (widget),
                                                   GTK_UPDATE_DELAYED);

                      if (!ret)
                        gtk_range_set_value (GTK_RANGE (widget), ctrl.value);

                      g_signal_connect (G_OBJECT (widget),
                                        "value-changed",
                                        G_CALLBACK (range_changed),
                                        GUINT_TO_POINTER (queryctrl.id));
                    }
                  else
                    {
                      gchar *str = g_strdup_printf ("fixme input %u %d %x",
                                                    ctrl.value, ctrl.value,
                                                    ctrl.value);
                      widget = gtk_label_new (str);
                      g_free (str);
                    }
                }
            }
        }
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 3);

#if 0
      if (queryctrl.type == V4L2_CTRL_TYPE_INTEGER)
        gtk_list_store_set (liststore, &iter,
                            COLUMN_VALUE, "integer",
                            -1);
      if (queryctrl.type == V4L2_CTRL_TYPE_BUTTON)
        gtk_list_store_set (liststore, &iter,
                            COLUMN_VALUE, "button",
                            -1);
#endif

//      if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
//          && queryctrl.type == V4L2_CTRL_TYPE_MENU)
//        enumerate_menu (fd, &queryctrl);

      if (widget)
        gtk_box_pack_end (GTK_BOX (hbox), widget, TRUE, TRUE, 3);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

  if (hbox)
    {
      gtk_widget_show_all (vbox);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
                                             vbox);
      g_object_set_data (G_OBJECT (scrolled), "vbox", vbox);
    }
  else
    g_object_unref (vbox);
}

static void
output_changed (GtkComboBox *combo,
                gpointer     user_data)
{
  gint       fd = GPOINTER_TO_INT (user_data);
  GtkWidget *scrolled;
  GtkWidget *vbox;

  if (fd == -1)
    return;

  scrolled = g_object_get_data (G_OBJECT (combo), "scrolled");
  vbox = g_object_get_data (G_OBJECT (scrolled), "vbox");
  if (vbox)
    {
      g_object_set_data (G_OBJECT (scrolled), "vbox", NULL);
      gtk_container_remove (GTK_CONTAINER (scrolled), vbox);
    }

  if (v4l2_set_output_by_index (fd, gtk_combo_box_get_active (combo)))
    return;

  enum_output_controls (fd, scrolled);
}

static void
input_changed (GtkComboBox *combo,
               gpointer     user_data)
{
  gint       fd = GPOINTER_TO_INT (user_data);
  GtkWidget *scrolled;
  GtkWidget *vbox;

  if (fd == -1)
    return;

  scrolled = g_object_get_data (G_OBJECT (combo), "scrolled");
  vbox = g_object_get_data (G_OBJECT (scrolled), "vbox");
  if (vbox)
    {
      g_object_set_data (G_OBJECT (scrolled), "vbox", NULL);
      gtk_container_remove (GTK_CONTAINER (scrolled), vbox);
    }

  if (v4l2_set_input_by_index (fd, gtk_combo_box_get_active (combo)))
    return;

  enum_output_controls (fd, scrolled);
}


static GtkWidget *main_window;
static GtkWidget *notebook;
static GtkTooltips *button_tooltips;


static void
control_dialogue (int fd)
{
  GtkWidget *vbox;
  gchar     *labels[] = { "Output", "Input" };
  gint       i;

  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (main_window), "crazy controls");

  g_signal_connect (G_OBJECT (main_window), "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (G_OBJECT (main_window), "delete-event",
                    G_CALLBACK (gtk_widget_destroy), NULL);
//                    G_CALLBACK (gtk_false), NULL);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (main_window), vbox);

  button_tooltips = gtk_tooltips_new ();

  /* a menu bar */
  {
    GtkWidget *menubar = gtk_menu_bar_new ();
    /* the file menu */
    {
      GtkWidget *file_item, *file_menu;
      GtkWidget *item_quit;

      file_item = gtk_menu_item_new_with_mnemonic ("_File");
      file_menu = gtk_menu_new ();
        item_quit = gtk_menu_item_new_with_mnemonic ("_Quit");

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (file_item), file_menu);
        gtk_menu_shell_append (GTK_MENU_SHELL (file_menu), item_quit);

      g_signal_connect_object (G_OBJECT (item_quit), "activate",
                               G_CALLBACK (gtk_widget_destroy),
                               G_OBJECT (main_window), G_CONNECT_SWAPPED);

      gtk_tooltips_set_tip (button_tooltips, item_quit,
                            "Way to hell",
                            "Lets you exit this application and gain "
                            "back valuable space on your screen");

      gtk_menu_bar_append (GTK_MENU_BAR (menubar), file_item);
    }
    /* help */
    {
      GtkWidget *help_item, *help_menu;
      GtkWidget *item_contents, *item_sep, *item_about;

      help_item = gtk_menu_item_new_with_mnemonic ("_Help");
      help_menu = gtk_menu_new ();
        item_contents = gtk_menu_item_new_with_mnemonic ("_Contents");
        item_sep = gtk_menu_item_new ();
        item_about = gtk_menu_item_new_with_mnemonic ("_About");

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (help_item), help_menu);
        gtk_menu_shell_append (GTK_MENU_SHELL (help_menu), item_contents);
        gtk_menu_shell_append (GTK_MENU_SHELL (help_menu), item_sep);
        gtk_menu_shell_append (GTK_MENU_SHELL (help_menu), item_about);

      gtk_menu_bar_append (GTK_MENU_BAR (menubar), help_item);
    }

    gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);
  }

  /* create a notebook and add it to vbox */
  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);
  gtk_widget_set_usize (GTK_WIDGET (notebook), 250, 150);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  for (i = 0; i < NOTEBOOK_COUNT; ++i)
    {
      GtkWidget *frame, *label;
      GtkWidget *combo;
      GtkWidget *vbox2;
      GtkWidget *scrolled;

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
      label = gtk_label_new (labels[i]);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      scrolled = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      if (i == NOTEBOOK_OUTPUT)
        {
          combo = v4l2_outputs_enumerate (fd);
          gtk_tooltips_set_tip (button_tooltips, label,
                                "All the output controls",
                                "Control all the output controls in "
                                "your system");
          gtk_tooltips_set_tip (button_tooltips, combo,
                                "What's an output you might ask",
                                "Select the output");
          g_signal_connect (G_OBJECT (combo),
                            "changed",
                            G_CALLBACK (output_changed), GINT_TO_POINTER (fd));
        }
      else if (i == NOTEBOOK_INPUT)
        {
          combo = v4l2_inputs_enumerate (fd);
          gtk_tooltips_set_tip (button_tooltips, label,
                                "All the input controls",
                                "Control all the input controls in "
                                "your system");
          gtk_tooltips_set_tip (button_tooltips, combo,
                                "I know what inputs are used for",
                                "You too?");
          g_signal_connect (G_OBJECT (combo),
                            "changed",
                            G_CALLBACK (input_changed), GINT_TO_POINTER (fd));
        }

      g_object_set_data (G_OBJECT (combo), "scrolled", scrolled);

      gtk_box_pack_start (GTK_BOX (vbox2), combo, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (vbox2), scrolled);

      /* add frame to notebook */
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

    }


  gtk_widget_show_all (main_window);
}

int
main (int argc, char *argv[])
{
  char devname[128];
  int  fd;

  snprintf (devname, sizeof (devname), "/dev/video0");
  _g_videofd = fd = LTV_CHECK (open (devname, O_RDWR));

  gtk_init (&argc, &argv);

  control_dialogue (fd);


  gtk_main ();

  LTV_CHECK (close (fd));

  return 0;
}
