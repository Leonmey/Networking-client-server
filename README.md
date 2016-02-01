# Networking-client-server
Cumulative assignment for Networking. Client server cloud database.

Will not run as it is missing some material provided by the instructor (provided us with a bug free version of our assignments 1 and 2 to use for assignment 3).

Client searches through a designated directory and coompares the contents to a redis database through a metadata server. Server responds with a list of the files that need to be updated. Client connects to a different server and uploads only the files that need updating via UDP. The server then updates the database.
