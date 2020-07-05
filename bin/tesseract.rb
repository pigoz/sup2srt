#!/usr/bin/env ruby
# frozen_string_literal: true

require 'rubygems'
require 'bundler/setup'
Bundler.require(:default)
require_relative '../src/ocr_jpn.rb'

def log(file, color, msg)
  puts "[#{File.basename(file)}] #{msg}".colorize(color)
end

def handle_image(image_path)
  xattr = Xattr.new(image_path)

  if xattr['ocr-text']
    log(image_path, :light_green, "skip: #{xattr['ocr-text']}")
    return
  end

  text = OcrJpn.new(image_path).call
  xattr['ocr-text'] = text
  log(image_path, :green, "done: #{text}")
rescue StandardError => e
  log(image_path, :red, "error: #{e.message}")
end

Dir.glob('./supdata/*.png').sort.each do |file|
  handle_image(file)
end
