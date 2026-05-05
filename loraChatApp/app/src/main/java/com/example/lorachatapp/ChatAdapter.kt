package com.example.lorachatapp

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.example.lorachatapp.databinding.ItemChatMeBinding
import com.example.lorachatapp.databinding.ItemChatOtherBinding

class ChatAdapter(private val messages: List<Message>) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    private val VIEW_TYPE_ME = 1
    private val VIEW_TYPE_OTHER = 2

    override fun getItemViewType(position: Int): Int {
        return if (messages[position].isMe) VIEW_TYPE_ME else VIEW_TYPE_OTHER
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        return if (viewType == VIEW_TYPE_ME) {
            val binding = ItemChatMeBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            SentMessageViewHolder(binding)
        } else {
            val binding = ItemChatOtherBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            ReceivedMessageViewHolder(binding)
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val message = messages[position]
        if (holder is SentMessageViewHolder) {
            holder.bind(message)
        } else if (holder is ReceivedMessageViewHolder) {
            holder.bind(message)
        }
    }

    override fun getItemCount(): Int = messages.size

    inner class SentMessageViewHolder(private val binding: ItemChatMeBinding) : RecyclerView.ViewHolder(binding.root) {
        fun bind(message: Message) {
            binding.textGchatMessageMe.text = message.content
            binding.textGchatTimestampMe.text = message.dateTime.split(" ")[1]
            binding.textGchatDateMe.text = message.dateTime.split(" ")[0]
        }
    }

    inner class ReceivedMessageViewHolder(private val binding: ItemChatOtherBinding) : RecyclerView.ViewHolder(binding.root) {
        fun bind(message: Message) {
            binding.textGchatMessageOther.text = message.content
            binding.textGchatTimestampOther.text = message.dateTime.split(" ")[1]
            binding.textGchatUserOther.text = message.sender
            binding.textGchatDateOther.text = message.dateTime.split(" ")[0]
        }
    }
}