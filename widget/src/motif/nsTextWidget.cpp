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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsXtEventHandler.h"

#include <Xm/Text.h>

#define DBG 0

extern int mIsPasswordCallBacksInstalled;

//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget(nsISupports *aOuter) : nsTextHelper(aOuter),
  mIsPasswordCallBacksInstalled(PR_FALSE),
  mMakeReadOnly(PR_FALSE),
  mMakePassword(PR_FALSE)
{
  //mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
void nsTextWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmTextWidgetClass, 
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
		                                XmNx, aRect.x,
		                                XmNy, aRect.y, 
                                    nsnull);

  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks();

  if (mMakeReadOnly) {
    SetReadOnly(PR_TRUE);
  }
  if (mMakePassword) {
    SetPassword(PR_TRUE);
  }

}

//-------------------------------------------------------------------------
void nsTextWidget::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

  if (aIID.Equals(kITextWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryInterface(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnPaint(nsPaintEvent & aEvent)
{
  return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextWidget::OnResize(nsRect &aWindowRect)
{
  return PR_FALSE;
}

//--------------------------------------------------------------
void nsTextWidget::SetPassword(PRBool aIsPassword)
{
  if (mWidget == nsnull && aIsPassword) {
    mMakePassword = PR_TRUE;
    return;
  }

  if (aIsPassword) {
    if (!mIsPasswordCallBacksInstalled) {
      XtAddCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtAddCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_TRUE;
    }
  } else {
    if (mIsPasswordCallBacksInstalled) {
      XtRemoveCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtRemoveCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_FALSE;
    }
  }
}

//--------------------------------------------------------------
PRBool  nsTextWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  if (mWidget == nsnull && aReadOnlyFlag) {
    mMakeReadOnly = PR_TRUE;
    return PR_TRUE;
  }
  return nsTextHelper::SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void nsTextWidget::SetMaxTextLength(PRUint32 aChars)
{
  nsTextHelper::SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return nsTextHelper::GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::SetText(const nsString& aText)
{ 
  return nsTextHelper::SetText(aText);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return nsTextHelper::InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  nsTextWidget::RemoveText()
{
  nsTextHelper::RemoveText();
}

//--------------------------------------------------------------
void nsTextWidget::SelectAll()
{
  nsTextHelper::SelectAll();
}


//--------------------------------------------------------------
void  nsTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  nsTextHelper::SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  nsTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  nsTextHelper::GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  nsTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  nsTextHelper::SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::GetCaretPosition()
{
  return nsTextHelper::GetCaretPosition();
}

//--------------------------------------------------------------
PRBool nsTextWidget::AutoErase()
{
  return nsTextHelper::AutoErase();
}



//--------------------------------------------------------------
#define GET_OUTER() ((nsTextWidget*) ((char*)this - nsTextWidget::GetOuterOffset()))


//--------------------------------------------------------------
void nsTextWidget::AggTextWidget::SetMaxTextLength(PRUint32 aChars)
{
  GET_OUTER()->SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return GET_OUTER()->GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::SetText(const nsString& aText)
{ 
  return GET_OUTER()->SetText(aText);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return GET_OUTER()->InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::RemoveText()
{
  GET_OUTER()->RemoveText();
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetPassword(PRBool aIsPassword)
{
  GET_OUTER()->SetPassword(aIsPassword);
}

//--------------------------------------------------------------
PRBool  nsTextWidget::AggTextWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  GET_OUTER()->SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void nsTextWidget::AggTextWidget::SelectAll()
{
  GET_OUTER()->SelectAll();
}


//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  GET_OUTER()->SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  GET_OUTER()->GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  GET_OUTER()->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::GetCaretPosition()
{
  return GET_OUTER()->GetCaretPosition();
}

PRBool nsTextWidget::AggTextWidget::AutoErase()
{
  return GET_OUTER()->AutoErase();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsTextWidget, AggTextWidget);

