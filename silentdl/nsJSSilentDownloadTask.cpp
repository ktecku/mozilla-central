/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMSilentDownloadTask.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kISilentDownloadTaskIID, NS_IDOMSILENTDOWNLOADTASK_IID);

NS_DEF_PTR(nsIDOMSilentDownloadTask);

//
// SilentDownloadTask property ids
//
enum SilentDownloadTask_slots {
  SILENTDOWNLOADTASK_ID = -1,
  SILENTDOWNLOADTASK_URL = -2,
  SILENTDOWNLOADTASK_SCRIPT = -3,
  SILENTDOWNLOADTASK_STATE = -4,
  SILENTDOWNLOADTASK_ERRORMSG = -5,
  SILENTDOWNLOADTASK_NEXTBYTE = -6,
  SILENTDOWNLOADTASK_OUTFILE = -7
};

/***********************************************************************/
//
// SilentDownloadTask Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetSilentDownloadTaskProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSilentDownloadTask *a = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case SILENTDOWNLOADTASK_ID:
      {
        nsAutoString prop;
        if (NS_OK == a->GetId(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_URL:
      {
        nsAutoString prop;
        if (NS_OK == a->GetUrl(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_SCRIPT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetScript(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_STATE:
      {
        PRInt32 prop;
        if (NS_OK == a->GetState(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_ERRORMSG:
      {
        nsAutoString prop;
        if (NS_OK == a->GetErrorMsg(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_NEXTBYTE:
      {
        PRInt32 prop;
        if (NS_OK == a->GetNextByte(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SILENTDOWNLOADTASK_OUTFILE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOutFile(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// SilentDownloadTask Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetSilentDownloadTaskProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSilentDownloadTask *a = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case SILENTDOWNLOADTASK_STATE:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetState(prop);
        
        break;
      }
      case SILENTDOWNLOADTASK_ERRORMSG:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetErrorMsg(prop);
        
        break;
      }
      case SILENTDOWNLOADTASK_NEXTBYTE:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetNextByte(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// SilentDownloadTask finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeSilentDownloadTask(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// SilentDownloadTask enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateSilentDownloadTask(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// SilentDownloadTask resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveSilentDownloadTask(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Init
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskInit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);

    if (NS_OK != nativeThis->Init(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Init requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Remove
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Remove()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Remove requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Suspend
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskSuspend(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Suspend()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Suspend requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Resume
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskResume(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Resume()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function Resume requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DownloadNow
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskDownloadNow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->DownloadNow()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function DownloadNow requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DownloadSelf
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTaskDownloadSelf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSilentDownloadTask *nativeThis = (nsIDOMSilentDownloadTask*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->DownloadSelf(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function DownloadSelf requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for SilentDownloadTask
//
JSClass SilentDownloadTaskClass = {
  "SilentDownloadTask", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetSilentDownloadTaskProperty,
  SetSilentDownloadTaskProperty,
  EnumerateSilentDownloadTask,
  ResolveSilentDownloadTask,
  JS_ConvertStub,
  FinalizeSilentDownloadTask
};


//
// SilentDownloadTask class properties
//
static JSPropertySpec SilentDownloadTaskProperties[] =
{
  {"id",    SILENTDOWNLOADTASK_ID,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"url",    SILENTDOWNLOADTASK_URL,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"script",    SILENTDOWNLOADTASK_SCRIPT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"state",    SILENTDOWNLOADTASK_STATE,    JSPROP_ENUMERATE},
  {"errorMsg",    SILENTDOWNLOADTASK_ERRORMSG,    JSPROP_ENUMERATE},
  {"nextByte",    SILENTDOWNLOADTASK_NEXTBYTE,    JSPROP_ENUMERATE},
  {"outFile",    SILENTDOWNLOADTASK_OUTFILE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// SilentDownloadTask class methods
//
static JSFunctionSpec SilentDownloadTaskMethods[] = 
{
  {"Init",          SilentDownloadTaskInit,     3},
  {"Remove",          SilentDownloadTaskRemove,     0},
  {"Suspend",          SilentDownloadTaskSuspend,     0},
  {"Resume",          SilentDownloadTaskResume,     0},
  {"DownloadNow",          SilentDownloadTaskDownloadNow,     0},
  {"DownloadSelf",          SilentDownloadTaskDownloadSelf,     1},
  {0}
};


//
// SilentDownloadTask constructor
//
PR_STATIC_CALLBACK(JSBool)
SilentDownloadTask(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMSilentDownloadTask *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMSilentDownloadTaskIID, NS_IDOMSILENTDOWNLOADTASK_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("SilentDownloadTask", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMSilentDownloadTaskIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  // XXX We should be calling Init() on the instance

  result = nativeThis->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);
  if (NS_OK != result) {
    NS_RELEASE(nativeThis);
    return JS_FALSE;
  }

  owner->SetScriptObject((void *)obj);
  JS_SetPrivate(cx, obj, nativeThis);

  NS_RELEASE(owner);
  return JS_TRUE;
}

//
// SilentDownloadTask class initialization
//
nsresult NS_InitSilentDownloadTaskClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "SilentDownloadTask", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &SilentDownloadTaskClass,      // JSClass
                         SilentDownloadTask,            // JSNative ctor
                         0,             // ctor args
                         SilentDownloadTaskProperties,  // proto props
                         SilentDownloadTaskMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "SilentDownloadTask", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_NOT_INITED);
      JS_SetProperty(jscontext, constructor, "SDL_NOT_INITED", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_NOT_ADDED);
      JS_SetProperty(jscontext, constructor, "SDL_NOT_ADDED", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_STARTED);
      JS_SetProperty(jscontext, constructor, "SDL_STARTED", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_SUSPENDED);
      JS_SetProperty(jscontext, constructor, "SDL_SUSPENDED", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_COMPLETED);
      JS_SetProperty(jscontext, constructor, "SDL_COMPLETED", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_DOWNLOADING_NOW);
      JS_SetProperty(jscontext, constructor, "SDL_DOWNLOADING_NOW", &vp);

      vp = INT_TO_JSVAL(nsIDOMSilentDownloadTask::SDL_ERROR);
      JS_SetProperty(jscontext, constructor, "SDL_ERROR", &vp);

    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new SilentDownloadTask JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptSilentDownloadTask(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptSilentDownloadTask");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMSilentDownloadTask *aSilentDownloadTask;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitSilentDownloadTaskClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kISilentDownloadTaskIID, (void **)&aSilentDownloadTask);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &SilentDownloadTaskClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSilentDownloadTask);
  }
  else {
    NS_RELEASE(aSilentDownloadTask);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
