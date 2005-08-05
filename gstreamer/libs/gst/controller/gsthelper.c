/*
 * gst-helper.c
 * 
 * GObject convinience methods for using dynamic properties
 *
 */

#include "config.h"
#include "gst-controller.h"

#define GST_CAT_DEFAULT gst_controller_debug
GST_DEBUG_CATEGORY_EXTERN (GST_CAT_DEFAULT);

extern GQuark controller_key;

/**
 * gst_object_control_properties:
 * @object: the object of which some properties should be controlled
 * @var_args: %NULL terminated list of property names that should be controlled
 *
 * Convenience function for GObject
 *
 * Creates a GstController that allows you to dynamically control one, or more, GObject properties.
 * If the given GObject already has a GstController, it adds the given properties to the existing 
 * controller and returns that controller.
 *
 * Returns: The GstController with which the user can control the given properties dynamically or NULL if
 * one or more of the given properties aren't available, or cannot be controlled, for the given element.
 */
GstController *
gst_object_control_properties (GObject * object, ...)
{
  GstController *ctrl;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  va_list var_args;

  va_start (var_args, object);
  ctrl = gst_controller_new_valist (object, var_args);
  va_end (var_args);
  return (ctrl);
}

/**
 * gst_object_uncontrol_properties:
 * @object: the object of which some properties should not be controlled anymore
 * @var_args: %NULL terminated list of property names that should be controlled
 *
 * Convenience function for GObject
 *
 * Removes the given element's properties from it's controller
 *
 * Returns: %FALSE if one of the given property names isn't handled by the
 * controller, %TRUE otherwise
 */
gboolean
gst_object_uncontrol_properties (GObject * object, ...)
{
  gboolean res = FALSE;
  GstController *ctrl;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  if ((ctrl = g_object_get_qdata (object, controller_key))) {
    va_list var_args;

    va_start (var_args, object);
    res = gst_controller_remove_properties_valist (ctrl, var_args);
    va_end (var_args);
  }
  return (res);
}

/**
 * gst_object_get_controller:
 * @object: the object that has controlled properties
 *
 * Returns: the controller handling some of the given element's properties,
 * %NULL if no controller
 */
GstController *
gst_object_get_controller (GObject * object)
{
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  return (g_object_get_qdata (object, controller_key));
}

/**
 * gst_object_set_controller:
 * @object: the object that should get the controller
 * @controller: the controller object to plug in
 *
 * Sets the controller on the given GObject
 *
 * Returns: %FALSE if the GObject already has an controller, %TRUE otherwise
 */
gboolean
gst_object_set_controller (GObject * object, GstController * controller)
{
  GstController *ctrl;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (controller, FALSE);

  ctrl = g_object_get_qdata (object, controller_key);
  g_return_val_if_fail (!ctrl, FALSE);
  g_object_set_qdata (object, controller_key, controller);
  return (TRUE);
}

/**
 * gst_object_sink_values:
 * @object: the object that has controlled properties
 * @timestamp: the time that should be processed
 *
 * Convenience function for GObject
 *
 * Returns: same thing as gst_controller_sink_values()
 */
gboolean
gst_object_sink_values (GObject * object, GstClockTime timestamp)
{
  GstController *ctrl = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);

  ctrl = g_object_get_qdata (object, controller_key);
  g_return_val_if_fail (ctrl, FALSE);
  return gst_controller_sink_values (ctrl, timestamp);
}

/**
 * gst_object_get_value_arrays:
 * @object: the object that has controlled properties
 * @timestamp: the time that should be processed
 * @value_arrays: list to return the control-values in
 *
 * Function to be able to get an array of values for one or more given element
 * properties.
 *
 * If the GstValueArray->values array in list nodes is NULL, it will be created 
 * by the function.
 * The type of the values in the array are the same as the property's type.
 *
 * The g_object_* functions are just convenience functions for GObject
 *
 * Returns: %TRUE if the given array(s) could be filled, %FALSE otherwise
 */
gboolean
gst_object_get_value_arrays (GObject * object, GstClockTime timestamp,
    GSList * value_arrays)
{
  GstController *ctrl;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);

  ctrl = g_object_get_qdata (object, controller_key);
  g_return_val_if_fail (ctrl, FALSE);
  return gst_controller_get_value_arrays (ctrl, timestamp, value_arrays);
}

/**
 * gst_object_get_value_array:
 * @self: the object that has controlled properties
 * @timestamp: the time that should be processed
 * @value_array: array to put control-values in
 *
 * Function to be able to get an array of values for one element properties
 *
 * If the GstValueArray->values array is NULL, it will be created by the function.
 * The type of the values in the array are the same as the property's type.
 *
 * The g_object_* functions are just convenience functions for GObject
 *
 * Returns: %TRUE if the given array(s) could be filled, %FALSE otherwise
 */
gboolean
gst_object_get_value_array (GObject * object, GstClockTime timestamp,
    GstValueArray * value_array)
{
  GstController *ctrl;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);

  ctrl = g_object_get_qdata (object, controller_key);
  g_return_val_if_fail (ctrl, FALSE);

  return gst_controller_get_value_array (ctrl, timestamp, value_array);
}
