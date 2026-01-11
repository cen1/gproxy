# Handshake dll

If you want to uniquely identify your GProxy build, so that your GHost bot does not accept just any GProxy version but only your specific build, you can implement custom code in this separate dll to return an identifier number which is appended to the handshake INIT packet. GHost can then reject a lobby join if the number is incorrect.

The implementation can simply return a "magic" number or can be a more complex calculation, depending on your needs.