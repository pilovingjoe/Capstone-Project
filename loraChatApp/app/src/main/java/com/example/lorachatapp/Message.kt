package com.example.lorachatapp

data class Message(
    val content: String,
    val sender: String,
    val receiver: String,
    val dateTime: String,
    val isMe: Boolean
)
{
    fun toSend(): String{
        return "$receiver;$sender;$dateTime;$content"
    }
}