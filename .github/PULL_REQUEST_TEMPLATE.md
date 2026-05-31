# Pull Request

## Summary

Describe the change briefly.

## Type of Change

Check all that apply:

- [ ] Bug fix
- [ ] Feature
- [ ] Documentation
- [ ] Build or packaging
- [ ] Tests
- [ ] Refactor
- [ ] Other

## Protocol Compatibility

Does this change affect the stdin/stdout JSON Lines protocol?

- [ ] No protocol change
- [ ] Compatible protocol change
- [ ] Breaking protocol change

If this is a breaking protocol change, explain why the runner protocol version should be incremented.

## Project Boundary

Confirm that this change preserves the intended project scope:

- [ ] No HTTP server added
- [ ] No TCP, UDP, or Unix socket listener added
- [ ] No daemon behavior added
- [ ] No model download logic added
- [ ] Logs remain on stderr
- [ ] Machine-readable events remain on stdout

## Testing

Describe how this was tested.

```sh

