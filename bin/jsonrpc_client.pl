#!/usr/bin/env perl
use strict;
use warnings;
use JSON::RPC::Client;
use JSON;

# Create a JSON-RPC client
my $client = JSON::RPC::Client->new;
$client->ua->timeout(10);

# Server URL
my $url = 'http://localhost:8080';

# Test the echo method
my $response = $client->call($url, {
    method  => 'echo',
    params  => { message => 'Hello, JSON-RPC!' },
});

if ($response && $response->is_success) {
    print "Echo Response: " . to_json($response->result, { pretty => 1 }) . "\n";
} else {
    print "Error calling echo: " . ($response ? $response->error_message : "No response") . "\n";
}

# Test the add method
$response = $client->call($url, {
    method  => 'add',
    params  => { a => 5, b => 3 },
});

if ($response && $response->is_success) {
    print "Add Response: " . $response->result . "\n";
} else {
    print "Error calling add: " . ($response ? $response->error_message : "No response") . "\n";
}
