; Flask routes: @app.route('/path')
(decorated_definition
  (decorator
    (call
      function: (attribute
        object: (identifier)
        attribute: (identifier) @route.decorator)
      arguments: (argument_list
        (string) @route.path))) @route.decorator.call
  definition: (function_definition
    name: (identifier) @route.handler))

; Flask with methods: @app.route('/path', methods=['POST'])
(decorated_definition
  (decorator
    (call
      function: (attribute
        object: (identifier)
        attribute: (identifier) @route.decorator)
      arguments: (argument_list
        (string) @route.path
        (keyword_argument
          name: (identifier) @methods.keyword
          value: (list
            (string) @route.method))))) @route.with.methods
  definition: (function_definition
    name: (identifier) @route.handler))

; FastAPI routes: @app.get('/path')
(decorated_definition
  (decorator
    (call
      function: (attribute
        object: (identifier)
        attribute: (identifier) @fastapi.method)
      arguments: (argument_list
        (string) @fastapi.path))) @fastapi.route
  definition: (function_definition
    name: (identifier) @fastapi.handler))