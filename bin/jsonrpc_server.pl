#!/usr/bin/env perl
use strict;
use warnings;
use JSON::RPC::Server::Daemon;
use JSON;

# Define the methods that the server will handle
my $server = JSON::RPC::Server::Daemon->new(
    LocalPort => 8080,
    ReuseAddr => 1,
);

# Example method: echo
$server->add_method('echo', sub {
    my ($params) = @_;
    return $params;
});

# Example method: add
$server->add_method('add', sub {
    my ($params) = @_;
    my $a = $params->{a} || 0;
    my $b = $params->{b} || 0;
    return $a + $b;
});

print "JSON-RPC Server started on port 8080\n";
$server->start;
