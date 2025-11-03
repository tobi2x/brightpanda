; HTTP client calls: requests.get("url"), httpx.post("url")
(call
  function: (attribute
    object: (identifier) @http.client.lib
    attribute: (identifier) @http.client.method)
  arguments: (argument_list
    (string) @http.client.url))