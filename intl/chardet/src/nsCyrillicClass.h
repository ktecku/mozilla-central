/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCyrillicClass_h__
#define nsCyrillicClass_h__
/* PLEASE DO NOT EDIT THIS FILE DIRECTLY. THIS FILE IS GENERATED BY 
   GenCyrllicClass found in mozilla/intl/chardet/tools
 */
static PRUint8 KOI8Map [128] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16, 
 17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32, 
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16, 
 17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32, 
};
static PRUint8 CP1251Map [128] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
};
static PRUint8 IBM866Map [128] = {
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
};
static PRUint8 ISO88595Map [128] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
};
static PRUint8 MacCyrillicMap [128] = {
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,  18, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  18, 
  2,   3,  24,   8,   5,   6,  23,  27,  10,  11,  12,  13,  14,  15,  16,  17, 
 19,  20,  21,  22,   7,   9,   4,  31,  28,  30,  32,  26,  25,  29,   1,   0, 
};
#endif
