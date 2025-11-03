; Function definitions
(function_definition
  name: (identifier) @function.name
  parameters: (parameters) @function.params
  body: (block) @function.body) @function.def

; Method definitions (functions inside classes)
(class_definition
  name: (identifier) @class.name
  body: (block
    (function_definition
      name: (identifier) @method.name
      parameters: (parameters) @method.params) @method.def))

; Decorators (useful for finding route handlers)
(decorated_definition
  (decorator
    (identifier) @decorator.name) @decorator
  definition: (function_definition
    name: (identifier) @decorated.function.name) @decorated.function)