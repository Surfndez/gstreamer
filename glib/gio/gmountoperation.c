/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <string.h>

#include "gmountoperation.h"
#include "gioenumtypes.h"
#include "gio-marshal.h"
#include "glibintl.h"

#include "gioalias.h"

/** 
 * SECTION:gmountoperation
 * @short_description: Authentication methods for mountable locations
 * @include: gio/gio.h
 *
 * #GMountOperation provides a mechanism for authenticating mountable 
 * operations, such as loop mounting files, hard drive partitions or 
 * server locations. 
 *
 * Mounting operations are handed a #GMountOperation that then can use 
 * if they require any privileges or authentication for their volumes 
 * to be mounted (e.g. a hard disk partition or an encrypted filesystem), 
 * or if they are implementing a remote server protocol which requires 
 * user credentials such as FTP or WebDAV.
 *
 * Users should instantiate a subclass of this that implements all
 * the various callbacks to show the required dialogs, such as 
 * #GtkMountOperation.
 **/

G_DEFINE_TYPE (GMountOperation, g_mount_operation, G_TYPE_OBJECT);

enum {
  ASK_PASSWORD,
  ASK_QUESTION,
  REPLY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GMountOperationPrivate {
  char *password;
  char *user;
  char *domain;
  gboolean anonymous;
  GPasswordSave password_save;
  int choice;
};

enum {
  PROP_0,
  PROP_USERNAME,
  PROP_PASSWORD,
  PROP_ANONYMOUS,
  PROP_DOMAIN,
  PROP_PASSWORD_SAVE,
  PROP_CHOICE
};

static void 
g_mount_operation_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GMountOperation *operation;

  operation = G_MOUNT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_USERNAME:
      g_mount_operation_set_username (operation, 
                                      g_value_get_string (value));
      break;
   
    case PROP_PASSWORD:
      g_mount_operation_set_password (operation, 
                                      g_value_get_string (value));
      break;

    case PROP_ANONYMOUS:
      g_mount_operation_set_anonymous (operation, 
                                       g_value_get_boolean (value));
      break;

    case PROP_DOMAIN:
      g_mount_operation_set_domain (operation, 
                                    g_value_get_string (value));
      break;

    case PROP_PASSWORD_SAVE:
      g_mount_operation_set_password_save (operation, 
                                           g_value_get_enum (value));
      break;

    case PROP_CHOICE:
      g_mount_operation_set_choice (operation, 
                                    g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void 
g_mount_operation_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GMountOperation *operation;
  GMountOperationPrivate *priv;

  operation = G_MOUNT_OPERATION (object);
  priv = operation->priv;
  
  switch (prop_id)
    {
    case PROP_USERNAME:
      g_value_set_string (value, priv->user);
      break;

    case PROP_PASSWORD:
      g_value_set_string (value, priv->password);
      break;

    case PROP_ANONYMOUS:
      g_value_set_boolean (value, priv->anonymous);
      break;

    case PROP_DOMAIN:
      g_value_set_string (value, priv->domain);
      break;

    case PROP_PASSWORD_SAVE:
      g_value_set_enum (value, priv->password_save);
      break;

    case PROP_CHOICE:
      g_value_set_int (value, priv->choice);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
g_mount_operation_finalize (GObject *object)
{
  GMountOperation *operation;
  GMountOperationPrivate *priv;

  operation = G_MOUNT_OPERATION (object);

  priv = operation->priv;
  
  g_free (priv->password);
  g_free (priv->user);
  g_free (priv->domain);

  G_OBJECT_CLASS (g_mount_operation_parent_class)->finalize (object);
}

static gboolean
reply_non_handled_in_idle (gpointer data)
{
  GMountOperation *op = data;

  g_mount_operation_reply (op, G_MOUNT_OPERATION_UNHANDLED);
  return FALSE;
}

static void
ask_password (GMountOperation *op,
	      const char      *message,
	      const char      *default_user,
	      const char      *default_domain,
	      GAskPasswordFlags flags)
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		   reply_non_handled_in_idle,
		   g_object_ref (op),
		   g_object_unref);
}
  
static void
ask_question (GMountOperation *op,
	      const char      *message,
	      const char      *choices[])
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		   reply_non_handled_in_idle,
		   g_object_ref (op),
		   g_object_unref);
}

static void
g_mount_operation_class_init (GMountOperationClass *klass)
{
  GObjectClass *object_class;
  
  g_type_class_add_private (klass, sizeof (GMountOperationPrivate));
 
  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = g_mount_operation_finalize;
  object_class->get_property = g_mount_operation_get_property;
  object_class->set_property = g_mount_operation_set_property;
  
  klass->ask_password = ask_password;
  klass->ask_question = ask_question;
  
  /**
   * GMountOperation::ask-password:
   * @op: a #GMountOperation requesting a password.
   * @message: string containing a message to display to the user.
   * @default_user: string containing the default user name.
   * @default_domain: string containing the default domain.
   * @flags: a set of #GAskPasswordFlags.
   * 
   * Emitted when a mount operation asks the user for a password.
   */
  signals[ASK_PASSWORD] =
    g_signal_new (I_("ask_password"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_password),
		  NULL, NULL,
		  _gio_marshal_VOID__STRING_STRING_STRING_FLAGS,
		  G_TYPE_NONE, 4,
		  G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ASK_PASSWORD_FLAGS);
		  
  /**
   * GMountOperation::ask-question:
   * @op: a #GMountOperation asking a question.
   * @message: string containing a message to display to the user.
   * @choices: an array of strings for each possible choice.
   * 
   * Emitted when asking the user a question and gives a list of 
   * choices for the user to choose from. 
   */
  signals[ASK_QUESTION] =
    g_signal_new (I_("ask_question"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_question),
		  NULL, NULL,
		  _gio_marshal_VOID__STRING_BOXED,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING, G_TYPE_STRV);
		  
  /**
   * GMountOperation::reply:
   * @op: a #GMountOperation.
   * @result: a #GMountOperationResult indicating how the request was handled
   * 
   * Emitted when the user has replied to the mount operation.
   */
  signals[REPLY] =
    g_signal_new (I_("reply"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, reply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  G_TYPE_MOUNT_OPERATION_RESULT);

  /**
   * GMountOperation:username:
   *
   * The user name that is used for authentication when carrying out
   * the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_USERNAME,
                                   g_param_spec_string ("username",
                                                        P_("Username"),
                                                        P_("The user name"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:password:
   *
   * The password that is used for authentication when carrying out
   * the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_PASSWORD,
                                   g_param_spec_string ("password",
                                                        P_("Password"),
                                                        P_("The password"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:anonymous:
   * 
   * Whether to use an anonymous user when authenticating.
   */
  g_object_class_install_property (object_class,
                                   PROP_ANONYMOUS,
                                   g_param_spec_boolean ("anonymous",
                                                         P_("Anonymous"),
                                                         P_("Whether to use an anonymous user"),
                                                         FALSE,
                                                         G_PARAM_READWRITE|
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:domain:
   *
   * The domain to use for the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_DOMAIN,
                                   g_param_spec_string ("domain",
                                                        P_("Domain"),
                                                        P_("The domain of the mount operation"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:password-save:
   *
   * Determines if and how the password information should be saved. 
   */ 
  g_object_class_install_property (object_class,
                                   PROP_PASSWORD_SAVE,
                                   g_param_spec_enum ("password-save",
                                                      P_("Password save"),
                                                      P_("How passwords should be saved"),
                                                      G_TYPE_PASSWORD_SAVE,
                                                      G_PASSWORD_SAVE_NEVER,
                                                      G_PARAM_READWRITE|
                                                      G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:choice:
   *
   * The index of the user's choice when a question is asked during the 
   * mount operation. See the #GMountOperation::ask-question signal.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_CHOICE,
                                   g_param_spec_int ("choice",
                                                     P_("Choice"),
                                                     P_("The users choice"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
g_mount_operation_init (GMountOperation *operation)
{
  operation->priv = G_TYPE_INSTANCE_GET_PRIVATE (operation,
						 G_TYPE_MOUNT_OPERATION,
						 GMountOperationPrivate);
}

/**
 * g_mount_operation_new:
 * 
 * Creates a new mount operation.
 * 
 * Returns: a #GMountOperation.
 **/
GMountOperation *
g_mount_operation_new (void)
{
  return g_object_new (G_TYPE_MOUNT_OPERATION, NULL);
}

/**
 * g_mount_operation_get_username
 * @op: a #GMountOperation.
 * 
 * Get the user name from the mount operation.
 *
 * Returns: a string containing the user name.
 **/
const char *
g_mount_operation_get_username (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->user;
}

/**
 * g_mount_operation_set_username:
 * @op: a #GMountOperation.
 * @username: input username.
 *
 * Sets the user name within @op to @username.
 **/
void
g_mount_operation_set_username (GMountOperation *op,
				const char      *username)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->user);
  op->priv->user = g_strdup (username);
  g_object_notify (G_OBJECT (op), "username");
}

/**
 * g_mount_operation_get_password:
 * @op: a #GMountOperation.
 *
 * Gets a password from the mount operation. 
 *
 * Returns: a string containing the password within @op.
 **/
const char *
g_mount_operation_get_password (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->password;
}

/**
 * g_mount_operation_set_password:
 * @op: a #GMountOperation.
 * @password: password to set.
 * 
 * Sets the mount operation's password to @password.  
 *
 **/
void
g_mount_operation_set_password (GMountOperation *op,
				const char      *password)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->password);
  op->priv->password = g_strdup (password);
  g_object_notify (G_OBJECT (op), "password");
}

/**
 * g_mount_operation_get_anonymous:
 * @op: a #GMountOperation.
 * 
 * Check to see whether the mount operation is being used 
 * for an anonymous user.
 * 
 * Returns: %TRUE if mount operation is anonymous. 
 **/
gboolean
g_mount_operation_get_anonymous (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), FALSE);
  return op->priv->anonymous;
}

/**
 * g_mount_operation_set_anonymous:
 * @op: a #GMountOperation.
 * @anonymous: boolean value.
 * 
 * Sets the mount operation to use an anonymous user if @anonymous is %TRUE.
 **/  
void
g_mount_operation_set_anonymous (GMountOperation *op,
				 gboolean         anonymous)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;

  if (priv->anonymous != anonymous)
    {
      priv->anonymous = anonymous;
      g_object_notify (G_OBJECT (op), "anonymous");
    }
}

/**
 * g_mount_operation_get_domain:
 * @op: a #GMountOperation.
 * 
 * Gets the domain of the mount operation.
 * 
 * Returns: a string set to the domain. 
 **/
const char *
g_mount_operation_get_domain (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->domain;
}

/**
 * g_mount_operation_set_domain:
 * @op: a #GMountOperation.
 * @domain: the domain to set.
 * 
 * Sets the mount operation's domain. 
 **/  
void
g_mount_operation_set_domain (GMountOperation *op,
			      const char      *domain)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->domain);
  op->priv->domain = g_strdup (domain);
  g_object_notify (G_OBJECT (op), "domain");
}

/**
 * g_mount_operation_get_password_save:
 * @op: a #GMountOperation.
 * 
 * Gets the state of saving passwords for the mount operation.
 *
 * Returns: a #GPasswordSave flag. 
 **/  

GPasswordSave
g_mount_operation_get_password_save (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), G_PASSWORD_SAVE_NEVER);
  return op->priv->password_save;
}

/**
 * g_mount_operation_set_password_save:
 * @op: a #GMountOperation.
 * @save: a set of #GPasswordSave flags.
 * 
 * Sets the state of saving passwords for the mount operation.
 * 
 **/   
void
g_mount_operation_set_password_save (GMountOperation *op,
				     GPasswordSave    save)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;
 
  if (priv->password_save != save)
    {
      priv->password_save = save;
      g_object_notify (G_OBJECT (op), "password-save");
    }
}

/**
 * g_mount_operation_get_choice:
 * @op: a #GMountOperation.
 * 
 * Gets a choice from the mount operation.
 *
 * Returns: an integer containing an index of the user's choice from 
 * the choice's list, or %0.
 **/
int
g_mount_operation_get_choice (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), 0);
  return op->priv->choice;
}

/**
 * g_mount_operation_set_choice:
 * @op: a #GMountOperation.
 * @choice: an integer.
 *
 * Sets a default choice for the mount operation.
 **/
void
g_mount_operation_set_choice (GMountOperation *op,
			      int              choice)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;
  if (priv->choice != choice)
    {
      priv->choice = choice;
      g_object_notify (G_OBJECT (op), "choice");
    }
}

/**
 * g_mount_operation_reply:
 * @op: a #GMountOperation
 * @result: a #GMountOperationResult
 * 
 * Emits the #GMountOperation::reply signal.
 **/
void
g_mount_operation_reply (GMountOperation *op,
			 GMountOperationResult result)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_signal_emit (op, signals[REPLY], 0, result);
}

#define __G_MOUNT_OPERATION_C__
#include "gioaliasdef.c"
