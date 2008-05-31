#include "runtime.h"
#include "error.h"
#include "global.h"
#include "idhash.h"
#include "conversions.h"
#include "jsdbgapi.h"

/*
 * call-seq:
 *   global
 *
 * Returns the global object used for this context.
 */
static VALUE global(VALUE self)
{
  JohnsonRuntime* runtime;
  Data_Get_Struct(self, JohnsonRuntime, runtime);
  return convert_to_ruby(runtime, OBJECT_TO_JSVAL(runtime->global));
}

/*
 * call-seq:
 *   evaluate(script, filename=nil, linenum=nil)
 *
 * Evaluate +script+ with +filename+ using +linenum+
 */
static VALUE evaluate(int argc, VALUE* argv, VALUE self)
{
  VALUE script, filename, linenum;

  JohnsonRuntime* runtime;
  Data_Get_Struct(self, JohnsonRuntime, runtime);

  JSContext * context = johnson_get_current_context(runtime);
  JohnsonContext * johnson_context = OUR_CONTEXT(context);
  rb_scan_args( argc, argv, "12", &script, &filename, &linenum );

  // clean things up first
  johnson_context->ex = 0;
  memset(johnson_context->msg, 0, MAX_EXCEPTION_MESSAGE_SIZE);

  const char* filenamez = RTEST(filename) ? StringValueCStr(filename) : "none";
  int linenumi = RTEST(linenum) ? NUM2INT(linenum) : 1;

  jsval js;

  // FIXME: should be able to pass in the 'file' name
  JSBool ok = JS_EvaluateScript(context, runtime->global,
    StringValuePtr(script), (unsigned)StringValueLen(script), filenamez, (unsigned)linenumi, &js);

  if (!ok)
  {
    if (JS_IsExceptionPending(context))
    {
      // If there's an exception pending here, it's a syntax error.
      JS_GetPendingException(context, &johnson_context->ex);
      JS_ClearPendingException(context);
    }

    if (johnson_context->ex)
    {
      return rb_funcall(self, rb_intern("handle_js_exception"),
        1, convert_to_ruby(runtime, johnson_context->ex));
      
      // VALUE message, file, line, stack;
      // 
      // jsval js_message;
      // assert(JS_GetProperty(context->js, JSVAL_TO_OBJECT(context->ex), "message", &js_message));
      // message = convert_to_ruby(context, js_message);
      // 
      // jsval js_file;
      // assert(JS_GetProperty(context->js, JSVAL_TO_OBJECT(context->ex), "fileName", &js_file));
      // file = convert_to_ruby(context, js_file);
      // 
      // jsval js_line;
      // assert(JS_GetProperty(context->js, JSVAL_TO_OBJECT(context->ex), "lineNumber", &js_line));
      // line = convert_to_ruby(context, js_line);
      // 
      // jsval js_stack;
      // assert(JS_GetProperty(context->js, JSVAL_TO_OBJECT(context->ex), "stack", &js_stack));
      // stack = convert_to_ruby(context, js_stack);
      // 
      // return rb_funcall(self, rb_intern("handle_js_exception"),
      //   4, message, file, line, stack);
    }
    
    char* msg = johnson_context->msg;

    // toString() whatever the exception object is (if we have one)
    if (johnson_context->ex)
      msg = JS_GetStringBytes(JS_ValueToString(context, johnson_context->ex));

    return Johnson_Error_raise(msg);
  }

  return convert_to_ruby(runtime, js);
}

/*
 * call-seq:
 *   debugger=(debugger)
 *
 * Sets a debugger object
 */
static VALUE
set_debugger(VALUE self, VALUE debugger)
{
  JohnsonRuntime* runtime;
  JSDebugHooks* debug_hooks;

  Data_Get_Struct(self, JohnsonRuntime, runtime);
  Data_Get_Struct(debugger, JSDebugHooks, debug_hooks);

  JSContext * context = johnson_get_current_context(runtime);

  JS_SetInterrupt(          runtime->js,
                            debug_hooks->interruptHandler,
                            debug_hooks->interruptHandlerData);
  JS_SetNewScriptHook(      runtime->js,
                            debug_hooks->newScriptHook,
                            debug_hooks->newScriptHookData);
  JS_SetDestroyScriptHook(  runtime->js,
                            debug_hooks->destroyScriptHook,
                            debug_hooks->destroyScriptHookData);
  JS_SetDebuggerHandler(    runtime->js,
                            debug_hooks->debuggerHandler,
                            debug_hooks->debuggerHandlerData);
  JS_SetSourceHandler(      runtime->js,
                            debug_hooks->sourceHandler,
                            debug_hooks->sourceHandlerData);
  JS_SetExecuteHook(        runtime->js,
                            debug_hooks->executeHook,
                            debug_hooks->executeHookData);
  JS_SetCallHook(           runtime->js,
                            debug_hooks->callHook,
                            debug_hooks->callHookData);
  JS_SetObjectHook(         runtime->js,
                            debug_hooks->objectHook,
                            debug_hooks->objectHookData);
  JS_SetThrowHook(          runtime->js,
                            debug_hooks->throwHook,
                            debug_hooks->throwHookData);
  JS_SetDebugErrorHook(     runtime->js,
                            debug_hooks->debugErrorHook,
                            debug_hooks->debugErrorHookData);

  JS_SetContextDebugHooks(context, debug_hooks);

  return debugger;
}

static VALUE
initialize_native(VALUE self, VALUE UNUSED(options))
{
  JohnsonRuntime* runtime;
  Data_Get_Struct(self, JohnsonRuntime, runtime);
  
  bool gcthings_rooted_p = false;

  if ((runtime->js = JS_NewRuntime(0x100000))
    && (runtime->jsids = create_id_hash())
    && (runtime->rbids = create_id_hash())
  )
  {
    JS_SetRuntimePrivate(runtime->js, (void *)self);
    JSContext* context = johnson_get_current_context(runtime);
    if(
        (runtime->gcthings = JS_NewObject(context, NULL, 0, 0))
        &&(runtime->global = JS_GetGlobalObject(context))
        &&(gcthings_rooted_p = JS_AddNamedRoot(context, &(runtime->global), "runtime->global"))
    ) {
      return self;
    }
    if (gcthings_rooted_p)
      JS_RemoveRoot(context, &(runtime->gcthings));
  }


  if (runtime->rbids)
    JS_HashTableDestroy(runtime->rbids);

  if (runtime->jsids)
    JS_HashTableDestroy(runtime->jsids);

  if (runtime->js)
    JS_DestroyRuntime(runtime->js);
    
  return Johnson_Error_raise("Couldn't initialize the runtime!");
}

JSContext* johnson_get_current_context(JohnsonRuntime * runtime)
{
  JohnsonContext * context = NULL;
  VALUE self = (VALUE)JS_GetRuntimePrivate(runtime->js);
  Data_Get_Struct(rb_funcall(self, rb_intern("current_context"), 0), JohnsonContext, context);
  return context->js;
}

static void deallocate(JohnsonRuntime* runtime)
{
  JS_RemoveRoot(johnson_get_current_context(runtime), &(runtime->global));
  
  JSContext *context;
  JSContext *iterator = NULL;

  while ((context = JS_ContextIterator(runtime->js, &iterator)) != NULL)
    JS_DestroyContext(context);
  
  JS_DestroyRuntime(runtime->js);
  free(runtime);
}

static VALUE allocate(VALUE klass)
{
  JohnsonRuntime* runtime = calloc(1, sizeof(JohnsonRuntime));
  return Data_Wrap_Struct(klass, 0, deallocate, runtime);
}

void init_Johnson_SpiderMonkey_Runtime(VALUE spidermonkey)
{
  VALUE klass = rb_define_class_under(spidermonkey, "Runtime", rb_cObject);

  rb_define_alloc_func(klass, allocate);
  rb_define_private_method(klass, "initialize_native", initialize_native, 1);

  rb_define_method(klass, "global", global, 0);
  rb_define_method(klass, "evaluate", evaluate, -1);
  rb_define_method(klass, "debugger=", set_debugger, 1);
}