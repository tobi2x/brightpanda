; Import statements
(import_statement
  name: (dotted_name) @import.module)

(import_from_statement
  module_name: (dotted_name) @import.from.module)

; HTTP client calls: requests.get/post/etc
(call
  function: (attribute
    object: (identifier) @http.client.lib
    attribute: (identifier) @http.client.method)
  arguments: (argument_list
    (string) @http.client.url)) @http.client.call

; Function calls that might be service calls
(call
  function: (attribute
    object: (identifier) @service.call.object
    attribute: (identifier) @service.call.method)
  arguments: (argument_list) @service.call.args) @service.call