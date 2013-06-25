// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * Delete unary operator can't delete object to be referenced
 *
 * @path ch08/8.7/S8.7_A5_T1.js
 * @description Delete referenced object, var __ref = obj
 * @noStrict
 */

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (typeof(__ref) !== "undefined"){
    $ERROR('#1: typeof(__ref) === "undefined". Actual: ' + (typeof(__ref)));  
}; 
//
//////////////////////////////////////////////////////////////////////////////

var obj = new Object();
var __ref = obj;

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (typeof(__ref) === "undefined"){
    $ERROR('#2: obj = new Object(); var __ref = obj; typeof(__ref) !== "undefined"');
}; 
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#3
if (delete __ref !== false){
    $ERROR('#3: obj = new Object(); var __ref = obj; delete __ref === false. Actual: ' + (delete __ref));
};
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#4
if (typeof(__ref) !== "object"){
    $ERROR('#4: obj = new Object(); var __ref = obj; delete __ref; typeof(__ref) === "object". Actual: ' + (typeof(__ref)));
};
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#5
if (typeof(obj) !== "object"){
    $ERROR('#5: obj = new Object(); var __ref = obj; delete __ref; typeof(obj) === "object". Actual: ' + (typeof(obj)));
};
//
//////////////////////////////////////////////////////////////////////////////

