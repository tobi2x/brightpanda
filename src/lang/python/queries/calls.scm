; ============================================================
; Brightpanda Python Call Detection Query
; Detects external HTTP calls, internal service calls, and async patterns.
; Designed for Tree-sitter Python grammar
; ============================================================

; ------------------------------------------------------------
; IMPORTS
; ------------------------------------------------------------
(import_statement
  name: (dotted_name) @import.module)

(import_from_statement
  module_name: (dotted_name) @import.from.module)

; ------------------------------------------------------------
; SECTION 1 — HTTP CLIENT CALLS
; Matches:
;   requests.get("url")
;   httpx.post("url")
;   aiohttp.ClientSession().get("url")
;   session.post("url")
; ------------------------------------------------------------

; Base call pattern: requests.get("url")
(call
  function: (attribute
    object: (identifier) @http.client.lib
    attribute: (identifier) @http.client.method)
  arguments: (argument_list
    (string) @http.client.url)) @http.client.call
; Only match known HTTP libs
(#match? @http.client.lib "^(requests|httpx|aiohttp|session|client|api)$")
(#match? @http.client.method "^(get|post|put|delete|patch|head|options)$")

; Session-style calls:
;   session = requests.Session(); session.post(url)
(call
  function: (attribute
    object: (identifier) @http.session.obj
    attribute: (identifier) @http.session.method)
  arguments: (argument_list
    (string) @http.session.url)) @http.session.call
(#match? @http.session.method "^(get|post|put|delete|patch|head|options)$")

; Async clients:
;   async with httpx.AsyncClient() as client: await client.get(url)
(await
  expression: (call
    function: (attribute
      object: (identifier) @async.http.client
      attribute: (identifier) @async.http.method)
    arguments: (argument_list
      (string) @async.http.url))) @async.http.call
(#match? @async.http.method "^(get|post|put|delete|patch|head|options)$")

; F-string or format() URLs:
(call
  function: (attribute
    object: (identifier) @fstring.http.lib
    attribute: (identifier) @fstring.http.method)
  arguments: (argument_list
    (f_string) @fstring.http.url)) @fstring.http.call
(#match? @fstring.http.lib "^(requests|httpx|aiohttp|session|client|api)$")
(#match? @fstring.http.method "^(get|post|put|delete|patch|head|options)$")

; ------------------------------------------------------------
; SECTION 2 — INTERNAL SERVICE CALLS
; Matches:
;   user_service.get_user()
;   repo.save_user(data)
;   email_client.send()
; ------------------------------------------------------------
(call
  function: (attribute
    object: (identifier) @service.call.object
    attribute: (identifier) @service.call.method)
  arguments: (argument_list) @service.call.args) @service.call

; Filter out known stdlib / non-service modules
(#not-match? @service.call.object "^(re|os|sys|json|math|time|datetime|logging|uuid|pathlib|subprocess|random|string|csv|shutil|itertools|functools|typing|traceback|pytest)$")

; ------------------------------------------------------------
; SECTION 3 — DATABASE & MESSAGE QUEUE CALLS
; Matches:
;   cursor.execute("SQL...")
;   db.session.query(User)
;   producer.publish(message)
; ------------------------------------------------------------

; SQL execution
(call
  function: (attribute
    object: (identifier) @db.call.object
    attribute: (identifier) @db.call.method)
  arguments: (argument_list
    (string) @db.call.query)) @db.call
(#match? @db.call.method "^(execute|executemany|query|fetchone|fetchall)$")

; MQ patterns
(call
  function: (attribute
    object: (identifier) @mq.call.object
    attribute: (identifier) @mq.call.method)
  arguments: (argument_list) @mq.call.args) @mq.call
(#match? @mq.call.object "^(producer|consumer|channel|queue|kafka|amqp|pubsub|sns|sqs)$")
(#match? @mq.call.method "^(send|publish|consume|produce|emit|push)$")

; ------------------------------------------------------------
; SECTION 4 — GENERIC FUNCTION CALLS (fallback, low confidence)
; Matches:
;   helper_fn()
;   utils.clean()
; ------------------------------------------------------------
(call
  function: (identifier) @generic.call.name
  arguments: (argument_list) @generic.call.args) @generic.call

; Exclude built-ins and trivial calls
(#not-match? @generic.call.name "^(print|len|range|int|str|float|dict|list|set|open|map|filter|zip|sum|min|max|any|all)$")

; ============================================================
; END OF FILE
; ============================================================
