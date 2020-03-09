#!/usr/bin/env ruby
# frozen_string_literal: true

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'ffi-xattr'
  gem 'colorize'
end

if ARGV.size != 2
  puts("usage: ./fix.rb 1 'new text'")
  exit
end

idx = ARGV[0].rjust(5, '0')
file = File.expand_path("./supdata/frame-#{idx}.png", __dir__)
Xattr.new(file)['ocr-text'] = ARGV[1]
