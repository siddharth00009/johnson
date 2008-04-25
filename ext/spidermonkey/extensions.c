#include "extensions.h"

static JSBool /* Object.defineProperty(target, name, value, flags) */
define_property(JSContext *js_context, JSObject* UNUSED(obj), uintN argc, jsval *argv, jsval *retval) {
  assert(argc > 1);
  char *name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

  // READ_ONLY | ITERABLE | NON_DELETABLE
  jsuint flags = argc > 3 ? (unsigned) JSVAL_TO_INT(argv[3]) : 0;

  *retval = JSVAL_VOID;
  return JS_DefineProperty(js_context, JSVAL_TO_OBJECT(argv[0]), name, argc > 2 ? argv[2] : JSVAL_VOID, NULL, NULL, flags);
}

bool init_spidermonkey_extensions(OurContext* context)
{
  jsval Object;
  
  if (JS_GetProperty(context->js, context->global, "Object", &Object))
  {
    if (JS_AddNamedRoot(context->js, &Object, "context.Object"))
    {
      if (JS_DefineFunction(context->js, JSVAL_TO_OBJECT(Object),
        "defineProperty", define_property, 4, 0)
        
        && JS_DefineProperty(context->js, JSVAL_TO_OBJECT(Object), "READ_ONLY",
          INT_TO_JSVAL(0x02), NULL, NULL, JSPROP_READONLY)
        
        && JS_DefineProperty(context->js, JSVAL_TO_OBJECT(Object), "ITERABLE",
          INT_TO_JSVAL(0x01), NULL, NULL, JSPROP_READONLY)
        
        && JS_DefineProperty(context->js, JSVAL_TO_OBJECT(Object), "NON_DELETABLE",
          INT_TO_JSVAL(0x04), NULL, NULL, JSPROP_READONLY))
      {
        JS_RemoveRoot(context->js, &Object);
        return true;
      }
      
      JS_RemoveRoot(context->js, &Object);
    }
  }
  
  return false;
}