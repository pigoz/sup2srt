#!/usr/bin/env ruby
# frozen_string_literal: true

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'srt'
  gem 'ffi-xattr'
  gem 'colorize'
end

def log(file, color, msg)
  puts "[#{File.basename(file)}] #{msg}".colorize(color)
end

srt = SRT::File.new

def to_time(seconds)
  seconds = seconds.to_f
  hhmmss = Time.at(seconds).utc.strftime('%H:%M:%S')
  ms = (seconds.modulo(1).round(3) * 1000).to_i
  "#{hhmmss},#{ms}"
end

Dir.glob('./supdata/*.png').sort.each_with_index do |file, idx|
  xattr = Xattr.new(file)
  start_time = xattr['start-time']
  end_time = xattr['end-time']
  text = xattr['ocr-text']

  if start_time.nil?
    log(file, :red, 'missing start-time')
    exit
  end

  if end_time.nil?
    log(file, :red, 'missing end-time')
    exit
  end

  if text.nil?
    log(file, :red, 'missing text')
    exit
  end

  srt.lines << SRT::Line.new(
    sequence: idx + 1,
    start_time: start_time.to_f,
    end_time: end_time.to_f,
    text: text
  )
end

puts(srt.to_s)
