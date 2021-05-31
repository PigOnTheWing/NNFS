../Server/nnfs_server localhost:12345 > /dev/null &
../Client/nnfs_client localhost:12345 < "$1" > "$2" &

server_pid=$(pidof -s nnfs_server)
client_pid=$(pidof -s nnfs_client)

echo "Server PID = $server_pid"
echo "Client PID = $client_pid"

[[ -n "$client_pid" ]] && wait "$client_pid"

sed -i -e 's/>> //g' "$2"

diff -Z "$3" "$2" > /dev/null && echo "Success" || echo "Fail"

[[ -n "$client_pid" ]] && kill "$client_pid" \
                       && echo "Killed client process" \
                       || echo "Failed to kill client process"
kill "$server_pid" && echo "Killed server process" \
                   || echo "Failed to kill server process"
