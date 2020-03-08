# frozen_string_literal: true

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'google-cloud-vision', require: 'google/cloud/vision'
  gem 'awesome_print', require: 'ap'
  gem 'ffi-xattr'
  gem 'colorize'
end

def log(file, color, msg)
  puts "[#{File.basename(file)}] #{msg}".colorize(color)
end

def ocr_image(image_path)
  xattr = Xattr.new(image_path)

  if xattr['ocr-text']
    log(image_path,
        :light_green,
        "skip: has ocr-text: '#{xattr['ocr-text']}'")
    return
  end

  response = Google::Cloud::Vision::ImageAnnotator.new.text_detection(
    image: image_path,
    max_results: 1 # optional, defaults to 10
  )

  response.responses.each do |res|
    text = res.text_annotations.first.description.strip
    xattr['ocr-text'] = text
    log(image_path,
        :green,
        "done: ocr'd text '#{text}'")
  end
rescue StandardError => e
  log(image_path,
      :red,
      "error: '#{e.message}'")
end

Dir.glob('./supdata/*.png') do |file|
  ocr_image(file)
end
